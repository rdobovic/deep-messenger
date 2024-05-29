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

// Callback functions
static void req_tran_cleanup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_txn_req *msg = phand->msg;
    prot_txn_req_free(msg);
}

static void req_tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    debug("Setting up transaction req");
    evbuffer_add(phand->buffer, prot_header(PROT_TRANSACTION_REQUEST), PROT_HEADER_LEN);
}

static void req_tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_txn_res *res;
    debug("Transaction request transmission finished");

    // Create new response handler and put it into queue
    res = prot_txn_res_new();
    prot_main_push_recv(pmain, &(res->hrecv));
    // Disable transmission until response is received
    prot_main_tran_enable(pmain, 0);
}

static void req_recv_cleanup(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_txn_req *msg = phand->msg;
    prot_txn_req_free(msg);
}

static void req_recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct evbuffer *buff;
    struct prot_txn_res *res;
    debug("Received transaction request");

    buff = bufferevent_get_input(pmain->bev);
    evbuffer_drain(buff, PROT_HEADER_LEN);

    res = prot_txn_res_new();
    prot_main_push_tran(pmain, &(res->htran));
    pmain->current_recv_done = 1;
}

// Allocate new prot transaction request handler
struct prot_txn_req * prot_txn_req_new(void) {
    struct prot_txn_req *msg;
    debug("Creating transaction request message object");

    msg = safe_malloc( sizeof(struct prot_txn_req), 
        "Failed to allocate transaction request msg");

    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_TRANSACTION_REQUEST;
    msg->hrecv.require_transaction = 0;
    msg->hrecv.handle_cb = req_recv_handle;
    msg->hrecv.cleanup_cb = req_recv_cleanup;

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_TRANSACTION_REQUEST;
    msg->htran.done_cb = req_tran_done;
    msg->htran.setup_cb = req_tran_setup;
    msg->htran.cleanup_cb = req_tran_cleanup;
    msg->htran.buffer = evbuffer_new();

    return msg;
}

// Free given transaction request handler
void prot_txn_req_free(struct prot_txn_req *msg) {
    evbuffer_free(msg->htran.buffer);
    free(msg);
}

/**
 * Transaction RESPONSE message
 */

// Callback functions
static void res_tran_cleanup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_txn_res *msg = phand->msg;
    prot_txn_res_free(msg);
}

static void res_tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_txn_res *msg = phand->msg;
    debug("Preparing txn res transmission");

    // Generate random transaction id
    if (RAND_bytes(msg->txn_id, TRANSACTION_ID_LEN) != 1) {
        sys_crash("openssl", "Failed to generate random bytes with error: %s",
            ERR_error_string(ERR_get_error(), NULL));
    }

    evbuffer_add(phand->buffer, prot_header(PROT_TRANSACTION_RESPONSE), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, msg->txn_id, TRANSACTION_ID_LEN);
}

static void res_tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_txn_res *msg = phand->msg;
    debug("Transaction response transmission finished");

    pmain->transaction_started = 1;
    memcpy(pmain->transaction_id, msg->txn_id, TRANSACTION_ID_LEN);
}

static void res_recv_cleanup(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_txn_res *msg = phand->msg;
    prot_txn_res_free(msg);
} 

static void res_recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct evbuffer *buff;
    debug("Received transaction response");

    buff = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(buff) < PROT_HEADER_LEN + TRANSACTION_ID_LEN)
        return;

    evbuffer_drain(buff, PROT_HEADER_LEN);
    evbuffer_remove(buff, pmain->transaction_id, TRANSACTION_ID_LEN);
    pmain->transaction_started = 1;
    prot_main_tran_enable(pmain, 1);
    pmain->current_recv_done = 1;
}

// Allocate new prot transaction response header
struct prot_txn_res * prot_txn_res_new(void) {
    struct prot_txn_res *msg;
    debug("Creating transaction response message object");

    msg = safe_malloc(sizeof(struct prot_txn_res), 
        "Failed to allocate transaction response msg");

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_TRANSACTION_RESPONSE;
    msg->htran.done_cb = res_tran_done;
    msg->htran.setup_cb = res_tran_setup;
    msg->htran.cleanup_cb = res_tran_cleanup;
    msg->htran.buffer = evbuffer_new();

    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_TRANSACTION_RESPONSE;
    msg->hrecv.handle_cb = res_recv_handle;
    msg->hrecv.cleanup_cb = res_recv_cleanup;
    msg->hrecv.require_transaction = 0;

    return msg;
}

// Free given transaction response header
void prot_txn_res_free(struct prot_txn_res *msg) {
    evbuffer_free(msg->htran.buffer);
    free(msg);
}