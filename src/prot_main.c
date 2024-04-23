#include <prot_main.h>
#include <sys_memory.h>
#include <socks5.h>
#include <queue.h>
#include <string.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <debug.h>
#include <prot_transaction.h>

// Internal bufferevent callbacks
static void prot_main_bev_read_cb(struct bufferevent *bev, void *ctx);
static void prot_main_bev_write_cb(struct bufferevent *bev, void *ctx);
static void prot_main_bev_event_cb(struct bufferevent *bev, short events, void *ctx);

// Socks5 done callback, called to setup
static void prot_main_socks5_cb(struct bufferevent *bev, enum socks5_errors err, void *attr);

// Call close callback and free protocol main
static void prot_main_fail(struct prot_main *pmain, enum prot_status_codes status)
{
    if (pmain->close_cb)
    {
        pmain->close_cb(pmain, status, pmain->cbarg);
    }
    debug("Main protocol handler failed with error: %s", prot_main_error_string(status));
    prot_main_free(pmain);
}

static void prot_main_done_check(struct prot_main *pmain)
{
    debug("Queue state recv(%d) tran(%d)", queue_get_length(pmain->recv_q), queue_get_length(pmain->tran_q));
    if (queue_is_empty(pmain->recv_q) && queue_is_empty(pmain->tran_q))
    {
        if (pmain->done_cb)
        {
            pmain->done_cb(pmain, pmain->cbarg);
        }
        debug("Main protocol handler done processing all messages");
    }
}

// Allocate new main protocol object
struct prot_main *prot_main_new(struct event_base *base)
{
    struct prot_main *pmain;

    pmain = safe_malloc(sizeof(struct prot_main), "Failed to allocate memory for prot_main struct");
    memset(pmain, 0, sizeof(struct prot_main));

    // Set event base
    pmain->event_base = base;

    // Allocate queues
    pmain->tran_q = queue_new(sizeof(struct prot_tran_handler));
    pmain->recv_q = queue_new(sizeof(struct prot_recv_handler));

    // Transmission is enabled by default
    pmain->tran_enabled = 1;
    // There are no active transmitters
    pmain->tran_in_progress = 0;
}

// Call cleanup for all in the queue and free main protocol object
void prot_main_free(struct prot_main *pmain)
{

    // Run cleanup function for all handlers in Receive queue
    while (!queue_is_empty(pmain->recv_q))
    {
        struct prot_recv_handler *phand = queue_peek(pmain->recv_q, 0);

        phand->cleanup_cb(phand);
        queue_dequeue(pmain->recv_q, NULL);
    }
    queue_free(pmain->recv_q);

    // Run cleanup function for all handlers in Transmit queue
    while (!queue_is_empty(pmain->tran_q))
    {
        struct prot_tran_handler *phand = queue_peek(pmain->tran_q, 0);

        phand->cleanup_cb(phand);
        queue_dequeue(pmain->tran_q, NULL);
    }
    queue_free(pmain->tran_q);

    if (pmain->bev)
        bufferevent_free(pmain->bev);
    free(pmain);
}

// Connect to given TOR client socks server and try to contact
// deep messenger instance on given onion address
void prot_main_connect(
    struct prot_main *pmain,
    const char *onion_address,
    struct sockaddr *socks_server_addr,
    size_t server_addr_size)
{
    pmain->bev = bufferevent_socket_new(pmain->event_base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_enable(pmain->bev, EV_READ | EV_WRITE);

    if (bufferevent_socket_connect(pmain->bev, socks_server_addr, server_addr_size) < 0)
    {
        prot_main_fail(pmain, PROT_ERR_SOCKS_CONN_FAIL);
        return;
    }

    socks5_connect_onion(
        pmain->bev, onion_address,
        DEEP_MESSENGER_PORT, prot_main_socks5_cb, pmain);
}

// Socks5 done callback, called to setup
static void prot_main_socks5_cb(struct bufferevent *bev, enum socks5_errors err, void *attr)
{
    struct evbuffer *out_buff;
    struct prot_main *pmain = attr;

    if (err > 0)
    {
        prot_main_fail(pmain, PROT_ERR_SOCKS_CONN_FAIL);
        return;
    }

    bufferevent_setcb(
        pmain->bev,
        prot_main_bev_read_cb,
        prot_main_bev_write_cb,
        prot_main_bev_event_cb,
        pmain);

    // If transmitter queue is not empty
    if (!queue_is_empty(pmain->tran_q))
    {
        struct prot_tran_handler *phand = queue_peek(pmain->tran_q, 0);

        out_buff = bufferevent_get_output(pmain->bev);

        // Add first transmitter to the buffer
        evbuffer_add_buffer(out_buff, phand->buffer);
        pmain->tran_in_progress = 1;
    }

    pmain->bev_ready = 1;
}

// Called from inside of callback function, notifies given pmain
// object that current receiver is done receiving
void prot_main_recv_done(struct prot_main *pmain)
{
    pmain->current_recv_done = 1;
}

// Push new message into transmission queue, returns zero on success
void prot_main_push_tran(struct prot_main *pmain, struct prot_tran_handler *phand)
{
    // Insert handler into queue
    queue_enqueue(pmain->tran_q, phand);

    // If bufferevent is ready and no transmission is in progress
    if (pmain->bev_ready && !pmain->tran_in_progress)
    {
        // Call write callback manually to try starting a new transmission
        prot_main_bev_write_cb(pmain->bev, pmain);
    }
}

// Push new message receiver into receiver queue, this is done when you are
// expecting message to arrive (response), returns zero on success
void prot_main_push_recv(struct prot_main *pmain, struct prot_recv_handler *phand)
{
    // Insert handler into queue
    queue_enqueue(pmain->recv_q, phand);
}

// Set callback functions
void prot_main_setcb(
    struct prot_main *pmain,
    prot_main_done_cb done_cb,
    prot_main_close_cb close_cb,
    void *cbarg)
{
    debug("Settings callbacks for pmain");
    pmain->cbarg = cbarg;
    pmain->done_cb = done_cb;
    pmain->close_cb = close_cb;
}

// Assign protocol connection handler to given bufferevent
void prot_main_assign(struct prot_main *pmain, struct bufferevent *bev)
{
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    bufferevent_setcb(
        bev,
        prot_main_bev_read_cb,
        prot_main_bev_write_cb,
        prot_main_bev_event_cb,
        pmain);

    pmain->bev = bev;
    pmain->bev_ready = 1;
}

// Called when there is data to read from bufferevent
static void prot_main_bev_read_cb(struct bufferevent *bev, void *ctx)
{
    int i;
    struct evbuffer *buff;
    struct prot_main *pmain = ctx;
    struct prot_recv_handler *phand;

    if (!pmain->bev_ready)
        return;

    debug("PMAIN READING");

    buff = bufferevent_get_input(pmain->bev);

    while (evbuffer_get_length(buff) > 0)
    {

        debug("Found something to read");

        if (!queue_is_empty(pmain->recv_q))
            phand = queue_peek(pmain->recv_q, 0);

        if (!pmain->message_check_done)
        {
            uint8_t *header;
            uint8_t message_code;

            // Check if header arrived
            if (evbuffer_get_length(buff) < PROT_HEADER_LEN)
                return;

            header = evbuffer_pullup(buff, PROT_HEADER_LEN);

            // Check version
            if (header[0] != DEEP_MESSENGER_PROTOCOL_VER)
            {
                prot_main_fail(pmain, PROT_ERR_PROTOCOL);
                return;
            }

            message_code = header[1];

            debug("Got new message with code %d", message_code);

            // If queue is empty try to get handler for given message type
            if (queue_is_empty(pmain->recv_q))
            {
                void *msg_object;

                msg_object = prot_handler_autogen(message_code, &phand, NULL);

                if (msg_object == NULL)
                {
                    prot_main_fail(pmain, PROT_ERR_INVALID_MSG);
                    return;
                }

                // Add new handler to the queue
                queue_enqueue(pmain->recv_q, phand);
                pmain->current_recv_done = 0;

                // Otherwise check if current handler is expecting this message type
                // if not fail
            }
            else
            {
                if (phand->msg_code != message_code)
                {
                    prot_main_fail(pmain, PROT_ERR_UNEXPECTED_MSG);
                    return;
                }
            }

            if (phand->require_transaction)
            {
                uint8_t *transaction_id;

                if (!pmain->transaction_started)
                {
                    prot_main_fail(pmain, PROT_ERR_TRANSACTION);
                    return;
                }

                // Transaction ID not arrived yet
                if (evbuffer_get_length(buff) < PROT_HEADER_LEN + PROT_TRANSACTION_ID_LEN)
                    return;

                header = evbuffer_pullup(buff, PROT_HEADER_LEN + PROT_TRANSACTION_ID_LEN);
                transaction_id = header + PROT_HEADER_LEN;

                // Compare if transaction id is valid
                for (i = 0; i < PROT_TRANSACTION_ID_LEN; i++)
                {
                    if (transaction_id[i] != pmain->transaction_id[i])
                    {
                        // Fail if not
                        prot_main_fail(pmain, PROT_ERR_TRANSACTION);
                        return;
                    }
                }
            }
            pmain->message_check_done = 1;
        }

        // Run handler
        pmain->current_recv_done = 0;
        phand->handle_cb(pmain, phand);

        // If handler is done run the handler cleanup and
        // remove handler from the queue
        if (pmain->current_recv_done)
        {
            phand->cleanup_cb(phand);
            queue_dequeue(pmain->recv_q, NULL);

            pmain->current_recv_done = 0;
            pmain->message_check_done = 0;

            prot_main_done_check(pmain);
        }
    }
}

static void prot_main_bev_write_cb(struct bufferevent *bev, void *ctx)
{
    struct evbuffer *buff;
    struct prot_main *pmain = ctx;
    struct prot_tran_handler *phand;

    buff = bufferevent_get_output(bev);

    debug("PMAIN WRITING");

    // If not all data from previous transmission is gone yet abort
    if (evbuffer_get_length(buff) > 0)
        return;

    // If queue is empty abort
    if (queue_is_empty(pmain->tran_q))
        return;

    phand = queue_peek(pmain->tran_q, 0);

    debug("Got handler to write");

    // If there is transmission in progress it is done now
    if (pmain->tran_in_progress)
    {
        debug("Transmission is done");
        // Notifiy handler that transmission is done and run the cleanup
        phand->done_cb(pmain, phand);
        phand->cleanup_cb(phand);

        queue_dequeue(pmain->tran_q, NULL);
        pmain->tran_in_progress = 0;

        prot_main_done_check(pmain);
        if (queue_is_empty(pmain->tran_q))
            return;

        phand = queue_peek(pmain->tran_q, 0);
    }

    // Abort if transmission is disabled
    if (!pmain->tran_enabled)
        return;

    debug("Writing data to output buffer");
    evbuffer_add_buffer(buff, phand->buffer);
    pmain->tran_in_progress = 1;
}

static void prot_main_bev_event_cb(struct bufferevent *bev, short events, void *ctx)
{
    struct prot_main *pmain = ctx;

    if (events & BEV_EVENT_TIMEOUT)
    {
        prot_main_fail(pmain, PROT_ERR_TIMEOUT);
        return;
    }

    if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF))
    {
        prot_main_fail(pmain, PROT_ERR_CONN_CLOSED);
        return;
    }
}

// Convert given error code to human readable error
const char *prot_main_error_string(enum prot_status_codes err_code)
{
    static char error_string[PROT_ERROR_MAX_LEN] = "Protocol: ";
    char *e = error_string + 10;

    switch (err_code)
    {
    case PROT_STATUS_OK:
        strcpy(e, "No error, success");
        break;

    case PROT_ERR_CONN_CLOSED:
        strcpy(e, "Connection was closed, or other connection error occured");
        break;
    case PROT_ERR_TIMEOUT:
        strcpy(e, "Timeout on bufferevent occured");
        break;
    case PROT_ERR_SOCKS_CONN_FAIL:
        strcpy(e, "Failed to connect to the socks server, or other socks error");
        break;
    case PROT_ERR_PROTOCOL:
        strcpy(e, "Protocol is invalid (invalid version)");
        break;
    case PROT_ERR_INVALID_MSG:
        strcpy(e, "Received message is invalid");
        break;
    case PROT_ERR_UNEXPECTED_MSG:
        strcpy(e, "Received unexpected message type");
        break;
    case PROT_ERR_TRANSACTION:
        strcpy(e, "Transaction is invalid, or not started but message type requires it");
        break;

    default:
        strcpy(e, "Unknown error code, this should never happen");
        break;
    }
}

// Used to enable/disable transmission on main protocol handler
void prot_main_tran_enable(struct prot_main *pmain, int yes)
{
    pmain->tran_enabled = yes;

    if (yes)
        prot_main_bev_write_cb(pmain->bev, pmain);
}

// Allocate new object for given message type and set given pointers
// to handlers for that message, if pointers are NULL they are not set
// function returns pointer to message object
void *prot_handler_autogen(
    enum prot_message_codes code,
    struct prot_recv_handler **phand_recv,
    struct prot_tran_handler **phand_tran)
{
    if (code == PROT_TRANSACTION_REQUEST)
    {
        struct prot_txn_req *msg;
        msg = prot_txn_req_new();

        if (phand_recv)
            *phand_recv = prot_txn_req_hrecv(msg);

        if (phand_tran)
            *phand_tran = prot_txn_req_htran(msg);

        return msg;
    }

    return NULL;
}

// Returns pointer to protocol header generated for given message type
// length of the header is equal to PROT_HEADER_LEN
const uint8_t *prot_header(enum prot_message_codes msg_code)
{
    static uint8_t header[PROT_HEADER_LEN] = {DEEP_MESSENGER_PROTOCOL_VER};
    header[1] = msg_code;

    return header;
}