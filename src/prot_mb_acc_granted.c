#include <stdint.h>
#include <hooks.h>
#include <onion.h>
#include <prot_main.h>
#include <db_mb_key.h>
#include <db_mb_account.h>
#include <prot_mb_account.h>
#include <sys_memory.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <db_options.h>
#include <buffer_crypto.h>
#include <debug.h>

// Called when transmission finished successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    db_mb_account_save(acc->db, acc->mb_acc);
}

// Called to free handler
static void tran_cleanup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    prot_mb_acc_granted_free(acc);
}

// Called to fill transmission buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    uint8_t mb_onion_priv_key[ONION_PRIV_KEY_LEN];

    evbuffer_add(phand->buffer, prot_header(PROT_MAILBOX_GRANTED), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
    evbuffer_add(phand->buffer, acc->mb_acc->mailbox_id, MAILBOX_ID_LEN);
    
    db_options_get_bin(acc->db, "onion_private_key", mb_onion_priv_key, ONION_PRIV_KEY_LEN);
    ed25519_buffer_sign(phand->buffer, 0, mb_onion_priv_key);
}

// Called to free handler
static void recv_cleanup(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;

    if (!phand->success)
        hook_list_call(pmain->hooks, PROT_MB_ACC_REGISTER_EV_FAIL, acc->cl_acc);

    prot_mb_acc_granted_free(acc);
}

// Called to handle incomming request
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    struct evbuffer *input;

    size_t message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN + 
        MAILBOX_ID_LEN + ED25519_SIGNATURE_LEN;

    input = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(input) < message_len)
        return;

    if (!ed25519_buffer_validate(input, message_len, acc->cl_acc->onion_key)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    evbuffer_drain(input, PROT_HEADER_LEN + TRANSACTION_ID_LEN);
    evbuffer_remove(input, acc->cl_acc->mailbox_id, MAILBOX_ID_LEN);

    hook_list_call(pmain->hooks, PROT_MB_ACC_REGISTER_EV_OK, acc->cl_acc);

    evbuffer_drain(input, ED25519_SIGNATURE_LEN);
    pmain->current_recv_done = 1;
}

// Create new account granted response handler,
// creates response for given registration request
struct prot_mb_acc * prot_mb_acc_granted_new(struct prot_mb_acc *acc_reg_req) {
    struct prot_mb_acc *acc;

    acc = safe_malloc(sizeof(struct prot_mb_acc), 
        "Failed to allocate memory for mailbox granted handler");

    // Copy data from registration request
    acc->db = acc_reg_req->db;
    acc->cl_acc = acc_reg_req->cl_acc ? acc_reg_req->cl_acc : NULL;
    acc->mb_acc = acc_reg_req->mb_acc ? acc_reg_req->mb_acc : NULL;

    acc->htran.msg = acc;
    acc->htran.msg_code = PROT_MAILBOX_GRANTED;
    acc->htran.done_cb = tran_done;
    acc->htran.setup_cb = tran_setup;
    acc->htran.cleanup_cb = tran_cleanup;
    acc->htran.buffer = evbuffer_new();

    acc->hrecv.msg = acc;
    acc->hrecv.msg_code = PROT_MAILBOX_GRANTED;
    acc->hrecv.require_transaction = 1;
    acc->hrecv.handle_cb = recv_handle;
    acc->hrecv.cleanup_cb = recv_cleanup;

    return acc;
}

// Free given mailbox account granted response
void prot_mb_acc_granted_free(struct prot_mb_acc *msg) {
    if (!msg) return;

    if (msg->cl_acc)
        free(msg->cl_acc);
    if (msg->mb_acc)
        db_mb_account_free(msg->mb_acc);

    evbuffer_free(msg->htran.buffer);
    free(msg);
}