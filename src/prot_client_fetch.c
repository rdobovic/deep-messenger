#include <sqlite3.h>
#include <prot_main.h>
#include <db_contact.h>
#include <prot_client_fetch.h>
#include <sys_memory.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <debug.h>
#include <buffer_crypto.h>
#include <prot_message_list.h>

// Called when fetch request is sent successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_client_fetch *msg = phand->msg;
    struct prot_message_list *msg_list;

    msg_list = prot_message_list_client_new(msg->db, msg->cont, NULL, 0);
    prot_main_push_recv(pmain, &(msg_list->hrecv));
    // Don't free the contact
    msg->cont = NULL;
}

// Called to free fetch request memory
static void tran_cleanup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_client_fetch *msg = phand->msg;
    debug("PCF Cleanup called");
    prot_client_fetch_free(msg);
}

// Called to serilize message and put it into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_client_fetch *msg = phand->msg;

    debug("Client fetch setup");
    
    evbuffer_add(phand->buffer, prot_header(PROT_CLIENT_FETCH), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
    evbuffer_add(phand->buffer, msg->cont->local_sig_key_pub, CLIENT_SIG_KEY_PUB_LEN);
    ed25519_buffer_sign(phand->buffer, 0, msg->cont->local_sig_key_priv);

    debug("Client fetch setup DONE");
}

// Called to free fetch request memory
static void recv_cleanup(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_client_fetch *msg = phand->msg;
    prot_client_fetch_free(msg);
}

// Called to handle incomming data
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_client_fetch *msg = phand->msg;
    struct evbuffer *input;
    struct evbuffer_ptr pos;
    struct db_contact *cont;

    int n_msgs;
    struct db_message **msgs;
    struct prot_message_list *msg_list;
    uint8_t sig_pub_key[CLIENT_SIG_KEY_PUB_LEN];

    size_t message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN +
        CLIENT_SIG_KEY_PUB_LEN + ED25519_SIGNATURE_LEN;

    debug("CLIENT FETCH HANDLE");

    input = bufferevent_get_input(pmain->bev);
    
    if (evbuffer_get_length(input) < message_len)
        return;

    debug("Received client fetch");

    evbuffer_ptr_set(input, &pos, PROT_HEADER_LEN + TRANSACTION_ID_LEN, EVBUFFER_PTR_SET);
    evbuffer_copyout_from(input, &pos, sig_pub_key, CLIENT_SIG_KEY_PUB_LEN);

    if (!ed25519_buffer_validate(input, message_len, sig_pub_key)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    if (!(cont = db_contact_get_by_rsk_pub(msg->db, sig_pub_key, NULL))) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    msgs = db_message_get_all(msg->db, cont, DB_MESSAGE_STATUS_UNDELIVERED, &n_msgs);
    debug(">>>>>>>>>>>>>>>>>> Found messages %d", n_msgs);
    msg_list = prot_message_list_client_new(msg->db, cont, msgs, n_msgs);
    prot_main_push_tran(pmain, &(msg_list->htran));

    evbuffer_drain(input, message_len);
    pmain->current_recv_done = 1;
}

// Allocate new client fetch handler object
struct prot_client_fetch * prot_client_fetch_new(sqlite3 *db, struct db_contact *cont) {
    struct prot_client_fetch *msg;

    msg = safe_malloc(sizeof(struct prot_client_fetch),
        "Failed to allocate memory for client fetch request handler");
    memset(msg, 0, sizeof(struct prot_client_fetch));

    msg->db = db;
    msg->cont = cont;

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_CLIENT_FETCH;
    msg->htran.done_cb = tran_done;
    msg->htran.setup_cb = tran_setup;
    msg->htran.cleanup_cb = tran_cleanup;
    msg->htran.buffer = evbuffer_new();

    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_CLIENT_FETCH;
    msg->hrecv.require_transaction = 1;
    msg->hrecv.handle_cb = recv_handle;
    msg->hrecv.cleanup_cb = recv_cleanup;

    return msg;
}

// Free client fetch handler
void prot_client_fetch_free(struct prot_client_fetch *msg) {
    debug("PCF FREE called");
    if (msg) {
        evbuffer_free(msg->htran.buffer);
        if (msg->cont)
            db_contact_free(msg->cont);
    }
    free(msg);
}