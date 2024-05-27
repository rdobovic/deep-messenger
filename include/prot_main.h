#ifndef _INCLUDE_PROT_MAIN_H_
#define _INCLUDE_PROT_MAIN_H_

#include <queue.h>
#include <stdint.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <constants.h>

#define PROT_QUEUE_LEN 32
#define PROT_ERROR_MAX_LEN 127

#define PROT_HEADER_LEN 2

// Application can work in one of following modes, some packets will be
// handled differently based on the choosen mode
enum prot_modes {
    PROT_MODE_CLIENT,
    PROT_MODE_MAILBOX,
};

// All message types defined by the Deep Messenger protocol
enum prot_message_codes {
    PROT_TRANSACTION_REQUEST  = 0x01,
    PROT_TRANSACTION_RESPONSE = 0x02,
    PROT_FRIEND_REQUEST       = 0x81,
    PROT_ACK_ONION            = 0x82,
    PROT_ACK_SIGNATURE        = 0x83,
    PROT_MESSAGE_CONTAINER    = 0x84,
    PROT_MAILBOX_REGISTER     = 0x85,
    PROT_MAILBOX_GRANTED      = 0x86,
    PROT_MAILBOX_FETCH        = 0x87,
    PROT_MAILBOX_SET_CONTACTS = 0x88,
    PROT_MAILBOX_DEL_ACCOUNT  = 0x89,
    PROT_MAILBOX_DEL_MESSAGES = 0x8A,
    PROT_CLIENT_FETCH         = 0x8B,
    PROT_MESSAGE_LIST         = 0x8C,
};

enum prot_status_codes {
    PROT_STATUS_OK,
    PROT_ERR_CONN_CLOSED,
    PROT_ERR_TIMEOUT,
    PROT_ERR_SOCKS_CONN_FAIL,
    PROT_ERR_PROTOCOL,
    PROT_ERR_INVALID_MSG,
    PROT_ERR_UNEXPECTED_MSG,
    PROT_ERR_TRANSACTION,
};

// Struct predefinition
struct prot_main;
struct prot_recv_handler;
struct prot_tran_handler;

// Callback used to free handle memory after it's done processing input
typedef void (*prot_recv_cleanup_cb)(struct prot_recv_handler *phand);
// Callback called to handle incomming data, must return 1 once it received and processed
// all the data, and 0 if not all data arrived yet
typedef void (*prot_recv_handle_cb)(struct prot_main *pmain, struct prot_recv_handler *phand);

struct prot_recv_handler {
    enum prot_message_codes msg_code;
    void *msg;
    // Cleanup functions can read this value to determine if message has been
    // processed successfully (1 = success, 0 = failure)
    int success;
    // If set to true prot_main will check first field after protocol
    // header for transaction ID, if ID is invalid it will close the connection
    int require_transaction;
    // Callback will be called to handle and parse incomming traffic
    prot_recv_handle_cb handle_cb;
    prot_recv_cleanup_cb cleanup_cb;
};

// Callback used to free handle memory after it's done processing input
typedef void (*prot_tran_cleanup_cb)(struct prot_tran_handler *phand);
// Called after transmission is done
typedef void (*prot_tran_done_cb)(struct prot_main *pmain, struct prot_tran_handler *phand);
// Called before transmission
typedef void (*prot_tran_setup_cb)(struct prot_main *pmain, struct prot_tran_handler *phand);

struct prot_tran_handler {
    enum prot_message_codes msg_code;
    void *msg;
    // Cleanup functions can read this value to determine if message has been
    // processed successfully (1 = success, 0 = failure)
    int success;
    // Buffer filled with protocol message
    struct evbuffer *buffer;
    // Callback to call once message is transmitted
    prot_tran_done_cb done_cb;
    prot_tran_setup_cb setup_cb;
    prot_tran_cleanup_cb cleanup_cb;
};

// Called when connecting and all messages are processed successfully
typedef void (*prot_main_done_cb)(struct prot_main *pmain, void *attr);

// Called when connection fails, or is closed
typedef void (*prot_main_close_cb)(struct prot_main *pmain, enum prot_status_codes status, void *attr);

// Main connection handler, attached to bufferevent connection
// to handle communication
struct prot_main {
    void *cbarg;                 // Pointer given to callbacks
    prot_main_done_cb done_cb;   // Called when all messages are processed and the transmition queue is empty
    prot_main_close_cb close_cb; // Called when connection is closed

    enum prot_modes mode;
    enum prot_status_codes status;

    struct bufferevent *bev;        // Bufferevent for this connection
    int bev_ready;                  // Set to 1 once bufferevent is ready
    struct event_base *event_base;  // Event base

    sqlite3 *db;                    // Database, used by some handlers on autogen

    int transaction_started;                          // Indicates if transaction has started
    uint8_t transaction_id[TRANSACTION_ID_LEN];  // Transaction ID associated with this connection

    // Set to 1 by the cb function when receiver processed the message
    int current_recv_done;
    // Used internally to know if there are active trasmitters
    int tran_in_progress;
    // Must be set to 1 if we want to transmit
    int tran_enabled;
    // Indicates that header of the current message has been checked and is OK
    // header includes version, message type and transaction id (if type requires it)
    int message_check_done;

    struct queue *tran_q; // Transmmitter queue
    struct queue *recv_q; // Receiver queue
};

// Allocate new main protocol object
struct prot_main *prot_main_new(struct event_base *base, sqlite3 *db);

// Call cleanup for all in the queue and free main protocol object
void prot_main_free(struct prot_main *pmain);

// Called from inside of callback function, notifies given pmain
// object that current receiver is done receiving
void prot_main_recv_done(struct prot_main *pmain);

// Connect to given TOR client socks server and try to contact
// deep messenger instance on given onion address
void prot_main_connect(
    struct prot_main *pmain,
    const char *onion_address,
    struct sockaddr *socks_server_addr,
    size_t server_addr_size
);

// Same as prot_main_connect but also use addrinfo to get sockaddr
// structure using specified port and address string
void prot_main_connect_parse(
    struct prot_main *pmain,
    const char *onion_address,
    uint16_t socks_server_port,
    const char *socks_server_addr
);

// Assign protocol connection handler to given bufferevent
void prot_main_assign(struct prot_main *pmain, struct bufferevent *bev);

// Push new message into transmission queue, returns zero on success
void prot_main_push_tran(struct prot_main *pmain, struct prot_tran_handler *phand);

// Push new message receiver into receiver queue, this is done when you are
// expecting message to arrive (response), returns zero on success
void prot_main_push_recv(struct prot_main *pmain, struct prot_recv_handler *phand);

// Set callback functions
void prot_main_setcb(
    struct prot_main *pmain,
    prot_main_done_cb done_cb,
    prot_main_close_cb close_cb,
    void *cbarg
);

// Used to enable/disable transmission on main protocol handler
void prot_main_tran_enable(struct prot_main *pmain, int yes);

// Allocate new receive handler for given message type, returns prot_recv_handler (on client)
struct prot_recv_handler *prot_handler_autogen_client(enum prot_message_codes code, sqlite3 *db);

// Allocate new receive handler for given message type, returns prot_recv_handler (on mailbox)
struct prot_recv_handler *prot_handler_autogen_mailbox(enum prot_message_codes code, sqlite3 *db);

// Convert given error code to human readable error
const char * prot_main_error_string(enum prot_status_codes err_code);

// Returns pointer to protocol header generated for given message type
// length of the header is equal to PROT_HEADER_LEN
const uint8_t * prot_header(enum prot_message_codes msg_code);

// Called from within tran/recv handler callbacks in case of error, main protocol
// handler will then free itself and close the connection
void prot_main_set_error(struct prot_main *pmain, enum prot_status_codes err_code);

#endif