#include <stdint.h>
#include <sqlite3.h>
#include <prot_main.h>
#include <sys_memory.h>
#include <db_contact.h>
#include <db_mb_account.h>
#include <db_mb_contact.h>
#include <prot_mb_set_contacts.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <array.h>
#include <buffer_crypto.h>
#include <prot_ack.h>
#include <onion.h>
#include <constants.h>
#include <db_options.h>
#include <debug.h>

// Called when ack is received after transmission
static void ack_received(int ack_success, struct prot_main *pmain, void *cbarg) {
    struct prot_mb_set_contacts *msg = cbarg;
        
    hook_list_call(msg->hooks, 
        ack_success ? PROT_MB_SET_CONTACTS_EV_OK : PROT_MB_SET_CONTACTS_EV_FAIL, msg);
    prot_mb_set_contacts_free(msg);
}

// Called when transmission finished successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_set_contacts *msg = phand->msg;
    struct prot_ack_ed25519 *ack;

    ack = prot_ack_ed25519_new(PROT_ACK_ONION, msg->onion_pub_key, NULL, ack_received, msg);
    prot_main_push_recv(pmain, &(ack->hrecv));
    phand->cleanup_cb = NULL;
}

// Called to free handler
static void tran_cleanup(struct prot_tran_handler *phand) {
    struct prot_mb_set_contacts *msg = phand->msg;

    hook_list_call(msg->hooks, PROT_MB_SET_CONTACTS_EV_FAIL, msg);
    prot_mb_set_contacts_free(msg);
}

// Called to fill transmission buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_set_contacts *msg = phand->msg;
    int i;
    uint16_t conts_len;

    evbuffer_add(phand->buffer, prot_header(PROT_MAILBOX_SET_CONTACTS), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
    evbuffer_add(phand->buffer, msg->cl_mb_id, MAILBOX_ID_LEN);

    conts_len = msg->n_cl_conts;
    conts_len = htons(conts_len);
    evbuffer_add(phand->buffer, &conts_len, sizeof(conts_len));

    for (i = 0; i < msg->n_cl_conts; i++) {
        evbuffer_add(phand->buffer, msg->cl_conts[i]->remote_sig_key_pub, CLIENT_SIG_KEY_PUB_LEN);
    }

    ed25519_buffer_sign(phand->buffer, 0, msg->cl_sig_priv_key);
}

// Called when ack is succesfully sent after receiveing account delete
static void ack_sent(int ack_success, struct prot_main *pmain, void *cbarg) {
    struct prot_mb_set_contacts *msg = cbarg;

    if (ack_success) {
        int i;
        for (i = 0; i < msg->n_mb_conts; i++)
            db_mb_contact_save(msg->db, msg->mb_conts[i]);
    }
    prot_mb_set_contacts_free(msg);
}

// Called to free handler
static void recv_cleanup(struct prot_recv_handler *phand) {
    struct prot_mb_set_contacts *msg = phand->msg;
    prot_mb_set_contacts_free(msg);
}

// Called to handle incomming request
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_mb_set_contacts *msg = phand->msg;
    int i;
    struct evbuffer *input;
    uint8_t *mailbox_id;
    uint16_t contacts_len;
    struct prot_ack_ed25519 *ack;
    uint8_t mb_onion_priv_key[ONION_PRIV_KEY_LEN];

    size_t message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN + 
        MAILBOX_ID_LEN + sizeof(contacts_len);

    input = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(input) < message_len)
        return;

    mailbox_id = evbuffer_pullup(input, message_len) + PROT_HEADER_LEN + TRANSACTION_ID_LEN;
    contacts_len = ntohs(*(uint16_t*)(mailbox_id + MAILBOX_ID_LEN));

    message_len += contacts_len * CLIENT_SIG_KEY_PUB_LEN + ED25519_SIGNATURE_LEN;

    if (evbuffer_get_length(input) < message_len)
        return;

    msg->mb_acc = db_mb_account_get_by_mbid(msg->db, mailbox_id, NULL);

    if (!msg->mb_acc || !ed25519_buffer_validate(input, message_len, msg->mb_acc->signing_pub_key)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    message_len -= contacts_len * CLIENT_SIG_KEY_PUB_LEN + ED25519_SIGNATURE_LEN;
    evbuffer_drain(input, message_len);

    msg->n_mb_conts = contacts_len;
    msg->mb_conts = array(struct db_mb_contact *);
    for (i = 0; i < contacts_len; i++) {
        struct db_mb_contact *cont;

        cont = db_mb_contact_new();
        cont->account_id = msg->mb_acc->id;
        evbuffer_remove(input, cont->signing_pub_key, CLIENT_SIG_KEY_PUB_LEN);
        array_set(msg->mb_conts, i, cont);
    }
    evbuffer_drain(input, ED25519_SIGNATURE_LEN);

    db_options_get_bin(msg->db, "mailbox_onion_private_key", mb_onion_priv_key, ONION_PRIV_KEY_LEN);
    ack = prot_ack_ed25519_new(PROT_ACK_ONION, NULL, mb_onion_priv_key, ack_sent, msg);
    prot_main_push_tran(pmain, &(ack->htran));

    pmain->current_recv_done = 1;
    phand->cleanup_cb = NULL;
}

// Allocate new set contacts handler, mailbox id and signing key should be provided
// only on the client side, on mailbox the should be NULL
struct prot_mb_set_contacts * prot_mb_set_contacts_new(
    sqlite3 *db,
    const char *onion_address,
    const uint8_t *cl_mb_id,
    const uint8_t *cl_sig_priv_key,
    struct db_contact **conts,
    int n_conts
) {
    struct prot_mb_set_contacts *msg;

    msg = safe_malloc(sizeof(struct prot_mb_set_contacts),
        "Failed to allocate memory for mailbox set contacts messsage handler");
    memset(msg, 0, sizeof(struct prot_mb_set_contacts));

    msg->db = db;
    msg->hooks = hook_list_new();

    if (onion_address && cl_mb_id && cl_sig_priv_key) {
        memcpy(msg->cl_mb_id, cl_mb_id, MAILBOX_ID_LEN);
        memcpy(msg->cl_sig_priv_key, cl_sig_priv_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);

        memcpy(msg->onion_address, onion_address, ONION_ADDRESS_LEN);
        onion_extract_key(onion_address, msg->onion_pub_key);

        msg->cl_conts = conts;
        msg->n_cl_conts = n_conts;
    }

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_MAILBOX_SET_CONTACTS;
    msg->htran.done_cb = tran_done;
    msg->htran.setup_cb = tran_setup;
    msg->htran.cleanup_cb = tran_cleanup;
    msg->htran.buffer = evbuffer_new();
    
    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_MAILBOX_SET_CONTACTS;
    msg->hrecv.require_transaction = 1;
    msg->hrecv.handle_cb = recv_handle;
    msg->hrecv.cleanup_cb = recv_cleanup;

    return msg;
}

// Free given set contacts message handler
void prot_mb_set_contacts_free(struct prot_mb_set_contacts *msg) {
    if (!msg) return;

    if (msg->hooks) 
        hook_list_free(msg->hooks);
    if (msg->mb_acc)
        db_mb_account_free(msg->mb_acc);
    if (msg->cl_conts)
        db_contact_free_all(msg->cl_conts, msg->n_cl_conts);

    if (msg->mb_conts) {
        int i;
        for (i = 0; i < msg->n_mb_conts; i++) {
            db_mb_contact_free(msg->mb_conts[i]);
        }
        array_free(msg->mb_conts);
    }

    free(msg);
}