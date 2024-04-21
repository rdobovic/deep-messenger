#include <socks5.h>
#include <sys_memory.h>
#include <stdint.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <debug.h>
#include <string.h>

/**
 * Internal function prototypes
 */

// Callback called by libevent when data arrives
static void socks5_read_cb(struct bufferevent *buffev, void *data);
// Callback called by bufferevent on event
static void socks5_event_cb(struct bufferevent *buffev, short events, void *data);
// Called when something fails to free bufferevent and socks5 data structures
static void socks5_internal_fail(struct socks5_data *data, enum socks5_errors err);

/**
 * Function definitions
 */

// Internal function, frees bufferevent and socks5 data structure and calls
// done callback with given error
void socks5_internal_fail(struct socks5_data *data, enum socks5_errors err) {
    bufferevent_free(data->buffev);
    data->done_cb(NULL, err, data->attr);
    free(data);
}

// Try to start socks5 connection on given bufferevent
void socks5_connect_onion(
    struct bufferevent *buffev,
    const uint8_t *onion_address,
    uint16_t port,
    socks5_done_cb done_cb,
    void *attr
) {
    int i;
    struct evbuffer *buff;
    struct socks5_data *data;

    // Create method/version selection packet to send
    uint8_t method_selection[] =
        { SOCKS5_VERSION, 0x01, SOCKS5_AUTH_METHOD };

    // Allocate and setup socks5 data structure
    data = safe_malloc(
        sizeof(struct socks5_data), "Failed to allocate socks5 data structure"
    );

    data->attr = attr;
    data->buffev = buffev;
    data->done_cb = done_cb;
    data->step = SOCKS5_STEP_AUTH;
    data->port = port;

    // Copy onion address to connect to
    for (i = 0; i < ONION_ADDRESS_LENGTH; i++) {
        data->onion_address[i] = onion_address[i];
    }

    // Setup read and event callbacks on bufferevent
    bufferevent_setcb(buffev, socks5_read_cb, NULL, socks5_event_cb, data);
    buff = bufferevent_get_output(buffev);
    // Add packet to buffer
    evbuffer_add(buff, method_selection, sizeof(method_selection));
}

// Callback called by libevent when data arrives
void socks5_read_cb(struct bufferevent *buffev, void *data) {
    struct socks5_data *dat = data;
    struct evbuffer *buff_out, *buff_in;

    buff_in = bufferevent_get_input(buffev);
    buff_out = bufferevent_get_output(buffev);

    // Handle server auth method response
    if (dat->step == SOCKS5_STEP_AUTH) {
        // Array to hold method selection response (length 2 bytes)
        uint8_t method_response[2];

        // Create first 4 (static) fields to send in connection request
        uint8_t request_header[] = 
            { SOCKS5_VERSION, SOCKS5_CONNECT_CMD, 0x00, SOCKS5_ADDRESS_DOMAIN };
        // Set address length byte
        uint8_t onion_address_len = ONION_ADDRESS_LENGTH;

        // When response arrives check if server accepted SOCKS5_AUTH_METHOD (nothing)
        // as auth method (it should since it's tor client)
        if (evbuffer_get_length(buff_in) < 2)
            return;

        evbuffer_remove(buff_in, method_response, 2);

        if (method_response[0] != SOCKS5_VERSION || method_response[1] != SOCKS5_AUTH_METHOD) {
            socks5_internal_fail(dat, SOCKS5_ERROR_AUTH_FAILED);
            return;
        }

        // Add static fields
        evbuffer_add(buff_out, request_header, sizeof(request_header));
        // Add onion address length field
        evbuffer_add(buff_out, &onion_address_len, 1);
        // Add onion address
        evbuffer_add(buff_out, dat->onion_address, onion_address_len);

        // Add port number
        dat->port = htons(dat->port);
        evbuffer_add(buff_out, &(dat->port), sizeof(dat->port));
        
        dat->step = SOCKS5_STEP_CONNECT;
    }

    // Handle server connect response
    else if (dat->step == SOCKS5_STEP_CONNECT) {
        size_t buff_len, address_len;
        uint8_t response_header[5];

        buff_len = evbuffer_get_length(buff_in);

        // When static fields and the first address byte arrive copy all data to
        // array and check if fields match
        if (buff_len < 5)
            return;

        evbuffer_copyout(buff_in, response_header, sizeof(response_header));

        if (response_header[0] != SOCKS5_VERSION) {
            socks5_internal_fail(dat, SOCKS5_ERROR_UNKNOWN);
            return;
        }

        if (response_header[1] != 0x00) {  // 0x00 for success, other for error
            socks5_internal_fail(dat, response_header[1]);
            return;
        }

        // Calculate bind address length based on address type
        switch (response_header[3]) {
            case SOCKS5_ADDRESS_IPv4:
                address_len = 4;
                break;

            case SOCKS5_ADDRESS_IPv6:
                address_len = 16;
                break;

            case SOCKS5_ADDRESS_DOMAIN:
                // Address length + length field
                address_len = response_header[4] + 1;
                break;

            default:
                socks5_internal_fail(dat, SOCKS5_ERROR_UNKNOWN);
                return;
        }

        // Header (static fields) + address + port
        if (buff_len < 4 + address_len + 2)
            return;

        // Drain all socks5 data from buffer and remove callbacks
        evbuffer_drain(buff_in, 4 + address_len + 2);
        bufferevent_setcb(buffev, NULL, NULL, NULL, NULL);
        // Execute done callback and free data structure
        dat->done_cb(buffev, SOCKS5_ERROR_NO_ERROR, dat->attr);
        free(dat);
    }
}

// Callback called by bufferevent on event
void socks5_event_cb(struct bufferevent *buffev, short events, void *data) {
    // An error occured on bufferevent socket
    if (events & BEV_EVENT_ERROR) {
        socks5_internal_fail(data, SOCKS5_ERROR_CONN_CLOSE);
    }
    // Connection closed
    if (events & BEV_EVENT_EOF) {
        socks5_internal_fail(data, SOCKS5_ERROR_CONN_CLOSE);
    }
}

// Generate human readable error string for socks5 error code
const char * socks5_error_string(enum socks5_errors err_code) {
    static char error_string[SOCKS5_ERROR_MAX_LEN] = "SOCKS5: ";
    char *e = error_string + 8;

    switch (err_code) {
        case SOCKS5_ERROR_NO_ERROR:
            strcpy(e, "No error, success");
            break;

        case SOCKS5_ERROR_SERVER_FAIL:
            strcpy(e, "Protocol(0x01): General SOCKS server fail"); 
            break;
        case SOCKS5_ERROR_CONN_NOT_ALLOWED:
            strcpy(e, "Protocol(0x02): Connection not allowed by the ruleset"); 
            break;
        case SOCKS5_ERROR_NETWORK_UNAVAIL:
            strcpy(e, "Protocol(0x03): Network unreachable"); 
            break;
        case SOCKS5_ERROR_HOST_UNAVAIL:
            strcpy(e, "Protocol(0x04): Host unreachable");
            break;
        case SOCKS5_ERROR_CONN_REFUSED:
            strcpy(e, "Protocol(0x05): Connection refused");
            break;
        case SOCKS5_ERROR_TTL_EXPIRED:
            strcpy(e, "Protocol(0x06): TTL expired");
            break;
        case SOCKS5_ERROR_CMD_UNSUPORTED:
            strcpy(e, "Protocol(0x07): Command not supported");
            break;
        case SOCKS5_ERROR_ADDRESS_TYPE_INVALID:
            strcpy(e, "Protocol(0x08): Address type not supported");
            break;

        case SOCKS5_ERROR_UNKNOWN:
            strcpy(e, "Unknown error, maybe server sent some invalid data");
            break;
        case SOCKS5_ERROR_CONN_CLOSE:
            strcpy(e, "Connection closed normally or failed because of socket error"); 
            break;
        case SOCKS5_ERROR_AUTH_FAILED:
            strcpy(e, "Server won't accept provided auth type (requires auth)");
            break;
        case SOCKS5_ERROR_CONN_TIMEOUT:
            strcpy(e, "Bufferevent timeout occured");
            break;

        default:
            strcpy(e, "Unknown error code, this should never happen");
            break;
    }

    return error_string;
}