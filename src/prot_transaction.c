#include <stdint.h>
#include <prot_main.h>
#include <prot_transaction.h>
#include <sys_memory.h>
#include <sys_crash.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <debug.h>
#include <constants.h>

/**
 * Transaction REQUEST message
 */

// Handler callbacks for transaction request transmission
static void prot_txn_req_tran_cleanup_cb(struct prot_tran_handler *phand);
static void prot_txn_req_tran_done_cb(struct prot_main *pmain, struct prot_tran_handler *phand);

// Handler callbacks fot transaction request receiver
static void prot_txn_req_recv_cleanup_cb(struct prot_recv_handler *phand);
static void prot_txn_req_recv_handle_cb(struct prot_main *pmain, struct prot_recv_handler *phand);

// Allocate new prot transaction request handler
struct prot_txn_req * prot_txn_req_new(void) {
    struct prot_txn_req *msg;
    debug("Creating transaction request message object");

    msg = safe_malloc( sizeof(struct prot_txn_req), 
        "Failed to allocate transaction request msg");

    msg->htran.buffer = evbuffer_new();
    return msg;
}

// Free given transaction request handler
void prot_txn_req_free(struct prot_txn_req *msg) {
    evbuffer_free(msg->htran.buffer);
    free(msg);
}

// Prepare transsion handler for given message
struct prot_tran_handler * prot_txn_req_htran(struct prot_txn_req *msg) {
    const uint8_t *hd;
    struct prot_tran_handler *htran = &(msg->htran);
    debug("Generating transmission handler for transaction request");

    htran->msg = msg;
    htran->msg_code = PROT_TRANSACTION_REQUEST;
    htran->done_cb = prot_txn_req_tran_done_cb;
    htran->cleanup_cb = prot_txn_req_tran_cleanup_cb;
    hd = prot_header(PROT_TRANSACTION_REQUEST);

    evbuffer_add(htran->buffer, prot_header(PROT_TRANSACTION_REQUEST), PROT_HEADER_LEN);
    return htran;
}

// Prepare receive for given message
struct prot_recv_handler * prot_txn_req_hrecv(struct prot_txn_req *msg) {
    struct prot_recv_handler *hrecv = &(msg->hrecv);
    debug("Generating receive handler for transaction request");

    hrecv->msg = msg;
    hrecv->msg_code = PROT_TRANSACTION_REQUEST;
    hrecv->require_transaction = 0;
    hrecv->handle_cb = prot_txn_req_recv_handle_cb;
    hrecv->cleanup_cb = prot_txn_req_recv_cleanup_cb;

    return hrecv;
}

// Callback functions
static void prot_txn_req_tran_cleanup_cb(struct prot_tran_handler *phand) {
    struct prot_txn_req *msg = phand->msg;
    prot_txn_req_free(msg);
}

static void prot_txn_req_tran_done_cb(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_txn_res *res;
    debug("Transaction request transmission finished");

    // Create new response handler and put it into queue
    res = prot_txn_res_new();
    prot_main_push_recv(pmain, prot_txn_res_hrecv(res));
    // Disable transmission until response is received
    pmain->tran_enabled = 0;
}

static void prot_txn_req_recv_cleanup_cb(struct prot_recv_handler *phand) {
    struct prot_txn_req *msg = phand->msg;
    prot_txn_req_free(msg);
}

static void prot_txn_req_recv_handle_cb(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct evbuffer *buff;
    struct prot_txn_res *res;
    debug("Received transaction request");

    buff = bufferevent_get_input(pmain->bev);
    evbuffer_drain(buff, PROT_HEADER_LEN);

    res = prot_txn_res_new();
    prot_main_push_tran(pmain, prot_txn_res_htran(res));
    pmain->current_recv_done = 1;
}


/**
 * Transaction RESPONSE message
 */

// Handler callbacks for transaction request transmission
static void prot_txn_res_tran_cleanup_cb(struct prot_tran_handler *phand);
static void prot_txn_res_tran_done_cb(struct prot_main *pmain, struct prot_tran_handler *phand);

// Handler callbacks fot transaction request receiver
static void prot_txn_res_recv_cleanup_cb(struct prot_recv_handler *phand);
static void prot_txn_res_recv_handle_cb(struct prot_main *pmain, struct prot_recv_handler *phand);

// Allocate new prot transaction response header
struct prot_txn_res * prot_txn_res_new(void) {
    struct prot_txn_res *msg;
    debug("Creating transaction response message object");

    msg = safe_malloc(sizeof(struct prot_txn_res), 
        "Failed to allocate transaction response msg");

    msg->htran.buffer = evbuffer_new();
    return msg;
}

// Free given transaction response header
void prot_txn_res_free(struct prot_txn_res *msg) {
    evbuffer_free(msg->htran.buffer);
    free(msg);
}

// Prepare transsion handler for given message
struct prot_tran_handler * prot_txn_res_htran(struct prot_txn_res *msg) {
    struct prot_tran_handler *htran = &(msg->htran);
    debug("Generating transmission handler for transaction response");

    htran->msg = msg;
    htran->msg_code = PROT_TRANSACTION_RESPONSE;
    htran->done_cb = prot_txn_res_tran_done_cb;
    htran->cleanup_cb = prot_txn_res_tran_cleanup_cb;

    // Generate random transaction id
    if (RAND_bytes(msg->txn_id, TRANSACTION_ID_LEN) != 1) {
        sys_crash("openssl", "Failed to generate random bytes with error: %s",
            ERR_error_string(ERR_get_error(), NULL));
    }

    evbuffer_add(htran->buffer, prot_header(PROT_TRANSACTION_RESPONSE), PROT_HEADER_LEN);
    evbuffer_add(htran->buffer, msg->txn_id, TRANSACTION_ID_LEN);
    return htran;
}

// Prepare receive for given message
struct prot_recv_handler * prot_txn_res_hrecv(struct prot_txn_res *msg) {
    struct prot_recv_handler *hrecv = &(msg->hrecv);
    debug("Generating receive handler for transaction response");

    hrecv->msg = msg;
    hrecv->msg_code = PROT_TRANSACTION_RESPONSE;
    hrecv->handle_cb = prot_txn_res_recv_handle_cb;
    hrecv->cleanup_cb = prot_txn_res_recv_cleanup_cb;
    hrecv->require_transaction = 0;

    return hrecv;
}

// Callback functions
static void prot_txn_res_tran_cleanup_cb(struct prot_tran_handler *phand) {
    struct prot_txn_res *msg = phand->msg;
    prot_txn_res_free(msg);
}

static void prot_txn_res_tran_done_cb(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_txn_res *msg = phand->msg;
    debug("Transaction response transmission finished");

    pmain->transaction_started = 1;
    memcpy(pmain->transaction_id, msg->txn_id, TRANSACTION_ID_LEN);
}

static void prot_txn_res_recv_cleanup_cb(struct prot_recv_handler *phand) {
    struct prot_txn_res *msg = phand->msg;
    prot_txn_res_free(msg);
} 

static void prot_txn_res_recv_handle_cb(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct evbuffer *buff;
    debug("Received transaction response");

    buff = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(buff) < PROT_HEADER_LEN + TRANSACTION_ID_LEN)
        return;

    evbuffer_drain(buff, PROT_HEADER_LEN);
    evbuffer_remove(buff, pmain->transaction_id, TRANSACTION_ID_LEN);
    pmain->tran_enabled = 1;
    pmain->current_recv_done = 1;
}