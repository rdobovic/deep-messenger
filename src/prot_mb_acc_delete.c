#include <stdint.h>
#include <hooks.h>
#include <onion.h>
#include <prot_main.h>
#include <prot_ack.h>
#include <db_mb_account.h>
#include <prot_mb_account.h>
#include <sys_memory.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <db_options.h>
#include <buffer_crypto.h>
#include <debug.h>

// Called when ack is received after transmission
static void ack_received(int ack_success, struct prot_main *pmain, void *cbarg) {
    struct prot_mb_acc *acc = cbarg;

    hook_list_call(pmain->hooks,
        ack_success ? PROT_MB_ACC_DELETE_EV_OK : PROT_MB_ACC_DELETE_EV_FAIL, acc->cl_acc);
    prot_mb_acc_delete_free(acc);
}

// Called when transmission finished successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    struct prot_ack_ed25519 *ack;

    ack = prot_ack_ed25519_new(PROT_ACK_ONION, acc->cl_acc->onion_key, NULL, ack_received, acc);
    prot_main_push_recv(pmain, &(ack->hrecv));

    phand->cleanup_cb = NULL;
}

// Called to free handler
static void tran_cleanup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;

    hook_list_call(pmain->hooks, PROT_MB_ACC_DELETE_EV_FAIL, acc->cl_acc);
    prot_mb_acc_delete_free(acc);
}

// Called to fill transmission buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;

    evbuffer_add(phand->buffer, prot_header(PROT_MAILBOX_DEL_ACCOUNT), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
    evbuffer_add(phand->buffer, acc->cl_acc->mailbox_id, MAILBOX_ID_LEN);
    
    ed25519_buffer_sign(phand->buffer, 0, acc->cl_acc->sig_priv_key);
}

// Called when ack is succesfully sent after receiveing account delete
static void ack_sent(int ack_success, struct prot_main *pmain, void *cbarg) {
    struct prot_mb_acc *acc = cbarg;

    debug("HERE BEFORE %d", acc->hrecv.require_transaction);

    if (ack_success)
        db_mb_account_delete(acc->db, acc->mb_acc);
    debug("HERE MID");
    prot_mb_acc_delete_free(acc);

    debug("HERE AFTER");
}

// Called to free handler
static void recv_cleanup(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    prot_mb_acc_delete_free(acc);
}

// Called to handle incomming request
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    uint8_t *mailbox_id;
    struct evbuffer *input;
    struct prot_ack_ed25519 *ack;
    uint8_t mb_onion_priv_key[MAILBOX_ACCOUNT_KEY_PRIV_LEN];

    int message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN + 
        MAILBOX_ID_LEN + ED25519_SIGNATURE_LEN;

    input = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(input) < message_len)
        return;

    mailbox_id = evbuffer_pullup(input, message_len - ED25519_SIGNATURE_LEN)
        + PROT_HEADER_LEN + TRANSACTION_ID_LEN;
    acc->mb_acc = db_mb_account_get_by_mbid(acc->db, mailbox_id, NULL);

    if (!acc->mb_acc || !ed25519_buffer_validate(input, message_len, acc->mb_acc->signing_pub_key)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    db_options_get_bin(acc->db, "mailbox_onion_private_key", mb_onion_priv_key, ONION_PRIV_KEY_LEN);
    ack = prot_ack_ed25519_new(PROT_ACK_ONION, NULL, mb_onion_priv_key, ack_sent, acc);
    prot_main_push_tran(pmain, &(ack->htran));

    evbuffer_drain(input, message_len);
    pmain->current_recv_done = 1;
    phand->cleanup_cb = NULL;
}

// Create new mailbox delete account handler, by sending this message client
// deletes its account and all associated messages
struct prot_mb_acc * prot_mb_acc_delete_new(sqlite3 *db, const char *onion_address, const uint8_t *mb_id, const uint8_t *mb_sig_priv_key) {
    struct prot_mb_acc *acc;

    acc = safe_malloc(sizeof(struct prot_mb_acc),
        "Failed to allocate memory for mb account delete handler");
    memset(acc, 0, sizeof(struct prot_mb_acc));

    acc->db = db;
    
    if (onion_address && mb_id && mb_sig_priv_key) {
        acc->cl_acc = safe_malloc(sizeof(struct prot_mb_acc_data), 
            "Failed to allocate memory for prot_mb_acc_data");

        memcpy(acc->cl_acc->mailbox_id, mb_id, MAILBOX_ID_LEN);
        memcpy(acc->cl_acc->sig_priv_key, mb_sig_priv_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);
        memcpy(acc->cl_acc->onion_address, onion_address, ONION_ADDRESS_LEN);
        onion_extract_key(onion_address, acc->cl_acc->onion_key);
    }

    acc->htran.msg = acc;
    acc->htran.msg_code = PROT_MAILBOX_DEL_ACCOUNT;
    acc->htran.done_cb = tran_done;
    acc->htran.setup_cb = tran_setup;
    acc->htran.cleanup_cb = tran_cleanup;
    acc->htran.buffer = evbuffer_new();

    acc->hrecv.msg = acc;
    acc->hrecv.msg_code = PROT_MAILBOX_DEL_ACCOUNT;
    acc->hrecv.require_transaction = 1;
    acc->hrecv.handle_cb = recv_handle;
    acc->hrecv.cleanup_cb = recv_cleanup;
    
    return acc;
}

// Free given mailbox account delete handler
void prot_mb_acc_delete_free(struct prot_mb_acc *msg) {
    if (!msg) return;

    if (msg->cl_acc)
        free(msg->cl_acc);
    if (msg->mb_acc)
        db_mb_account_free(msg->mb_acc);

    free(msg);
}