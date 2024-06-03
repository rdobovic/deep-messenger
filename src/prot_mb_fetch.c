#include <sqlite3.h>
#include <prot_main.h>
#include <prot_mb_fetch.h>
#include <db_mb_account.h>
#include <sys_memory.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <debug.h>
#include <db_options.h>
#include <buffer_crypto.h>
#include <prot_message_list.h>
#include <db_mb_message.h>

// Called when fetch request is sent successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_fetch *msg = phand->msg;
    struct prot_message_list *msg_list;

    msg_list = prot_message_list_client_new(msg->db, NULL, NULL, 0);
    prot_message_list_from(msg_list, PROT_MESSAGE_LIST_FROM_MAILBOX);
    prot_main_push_recv(pmain, &(msg_list->hrecv));
}

// Called to free fetch request memory
static void tran_cleanup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_fetch *msg = phand->msg;

    if (!phand->success)
        hook_list_call(pmain->hooks, PROT_MB_FETCH_EV_FAIL, NULL);
    prot_mb_fetch_free(msg);
}

// Called to serilize message and put it into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_fetch *msg = phand->msg;

    evbuffer_add(phand->buffer, prot_header(PROT_MAILBOX_FETCH), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
    evbuffer_add(phand->buffer, msg->mb_id, MAILBOX_ID_LEN);
    ed25519_buffer_sign(phand->buffer, 0, msg->mb_priv_sig_key);
}

// Called to free fetch request memory
static void recv_cleanup(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_mb_fetch *msg = phand->msg;
    prot_mb_fetch_free(msg);
}

// Called to handle incomming data
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_mb_fetch *msg = phand->msg;
    struct evbuffer *input;
    struct evbuffer_ptr pos;
    struct db_mb_account *acc;
    uint8_t mb_onion_priv_key[ONION_PRIV_KEY_LEN];

    int n_msgs;
    struct db_mb_message **msgs;
    struct prot_message_list *msg_list;
    uint8_t message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN +
        MAILBOX_ID_LEN + ED25519_SIGNATURE_LEN;
    
    debug("MB FETCH");

    input = bufferevent_get_input(pmain->bev);
    if (evbuffer_get_length(input) < message_len)
        return;

    debug("LENGTH OK");

    evbuffer_ptr_set(input, &pos, PROT_HEADER_LEN + TRANSACTION_ID_LEN, EVBUFFER_PTR_SET);
    evbuffer_copyout_from(input, &pos, msg->mb_id, MAILBOX_ID_LEN);

    acc = db_mb_account_get_by_mbid(msg->db, msg->mb_id, NULL);
    if (!acc || !ed25519_buffer_validate(input, message_len, acc->signing_pub_key)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    debug("ACCOUNT FOUND, SIG OK");

    msgs = db_mb_message_get_all(msg->db, acc, &n_msgs);
    debug("Found %d new messages to deliver", n_msgs);
    msg_list = prot_message_list_mailbox_new(msg->db, msgs, n_msgs);
    prot_main_push_tran(pmain, &(msg_list->htran));

    debug("PUSHED MSG LIST");

    evbuffer_drain(input, message_len);
    pmain->current_recv_done = 1;

    debug("DONE");
}

// Allocate new mailbox fetch handler object
struct prot_mb_fetch * prot_mb_fetch_new(sqlite3 *db) {
    struct prot_mb_fetch *msg;

    msg = safe_malloc(sizeof(struct prot_mb_fetch),
        "Failed to allocate memory for mailbox fetch request handler");
    memset(msg, 0, sizeof(struct prot_mb_fetch));

    msg->db = db;

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_MAILBOX_FETCH;
    msg->htran.done_cb = tran_done;
    msg->htran.setup_cb = tran_setup;
    msg->htran.cleanup_cb = tran_cleanup;
    msg->htran.buffer = evbuffer_new();

    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_MAILBOX_FETCH;
    msg->hrecv.require_transaction = 1;
    msg->hrecv.handle_cb = recv_handle;
    msg->hrecv.cleanup_cb = recv_cleanup;

    db_options_get_bin(db, "client_mailbox_id", msg->mb_id, MAILBOX_ID_LEN);
    db_options_get_bin(db, "client_mailbox_sig_pub_key", msg->mb_pub_sig_key, MAILBOX_ACCOUNT_KEY_PUB_LEN);
    db_options_get_bin(db, "client_mailbox_sig_priv_key", msg->mb_priv_sig_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);

    db_options_get_text(db, "client_mailbox_onion_address", msg->mb_onion_address, ONION_ADDRESS_LEN + 1);
    onion_extract_key(msg->mb_onion_address, msg->mb_onion_key);

    return msg;
}

// Free mailbox fetch handler
void prot_mb_fetch_free(struct prot_mb_fetch *msg) {
    if (msg) {
        evbuffer_free(msg->htran.buffer);
    }
    free(msg);
}