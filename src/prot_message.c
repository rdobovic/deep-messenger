#include <stdint.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <db_contact.h>
#include <db_message.h>
#include <db_mb_message.h>
#include <sys_memory.h>
#include <prot_main.h>
#include <prot_message.h>
#include <prot_ack.h>
#include <buffer_crypto.h>

static void ack_received_cb(int ack_success, struct prot_main *pmain, void *arg) {
    struct prot_message *msg = arg;

    // When ACK is received and this is client set message as sent
    if (ack_success && pmain->mode == PROT_MODE_CLIENT) {
        msg->dbmsg_client->status = DB_MESSAGE_STATUS_SENT;
        db_message_save(msg->db, msg->dbmsg_client);
    }

    db_message_free(msg->dbmsg_client);
    prot_message_free(msg);
}

// Called if message is sent successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_message *msg = phand->msg;
    struct prot_ack_ed25519 *ack;

    // Only client can sent a message outside the message list
    if (pmain->mode == PROT_MODE_CLIENT) {
        struct db_contact *cont;

        cont = db_contact_get_by_pk(msg->db, msg->dbmsg_client->contact_id, NULL);

        if (msg->to == PROT_MESSAGE_TO_CLIENT) {
            ack = prot_ack_ed25519_new(PROT_ACK_SIGNATURE, cont->remote_sig_key_pub, NULL, ack_received_cb, msg);
        }

        if (msg->to == PROT_MESSAGE_TO_MAILBOX) {
            uint8_t onion_key[ONION_PUB_KEY_LEN];

            onion_extract_key(cont->mailbox_onion, onion_key);
            ack = prot_ack_ed25519_new(PROT_ACK_ONION, onion_key, NULL, ack_received_cb, msg);
        }

        prot_main_push_recv(pmain, &(ack->hrecv));
        phand->cleanup_cb = NULL;
        db_contact_free(cont);
    }
}

// Free message handler object
static void tran_cleanup(struct prot_tran_handler *phand) {
    struct prot_message *msg = phand->msg;
    db_message_free(msg->dbmsg_client);
    prot_message_free(msg);
}

// Called to put message into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_message *msg = phand->msg;

    // Only client can send a message outside the message list
    if (pmain->mode == PROT_MODE_CLIENT) {
        uint8_t ctype;
        struct evbuffer *plain;
        struct db_contact *cont;

        plain = evbuffer_new();
        cont = db_contact_get_by_pk(msg->db, msg->dbmsg_client->contact_id, NULL);

        evbuffer_add(phand->buffer, prot_header(PROT_MESSAGE_CONTAINER), PROT_HEADER_LEN);
        evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
        evbuffer_add(phand->buffer, cont->mailbox_id, MAILBOX_ID_LEN);
        evbuffer_add(phand->buffer, cont->local_sig_key_pub, CLIENT_SIG_KEY_PUB_LEN);
        evbuffer_add(phand->buffer, msg->dbmsg_client->global_id, MESSAGE_ID_LEN);

        ctype = msg->dbmsg_client->type;
        evbuffer_add(plain, &ctype, sizeof(ctype));

        switch (ctype) {
            case DB_MESSAGE_TEXT:
                evbuffer_add(plain, msg->dbmsg_client->body_text, msg->dbmsg_client->body_text_len);
                break;
            case DB_MESSAGE_NICK:
                evbuffer_add(plain, msg->dbmsg_client->body_nick, msg->dbmsg_client->body_nick_len);
                break;
            case DB_MESSAGE_MBOX:
                evbuffer_add(plain, msg->dbmsg_client->body_mbox_id, MAILBOX_ID_LEN);
                evbuffer_add(plain, msg->dbmsg_client->body_mbox_onion, ONION_ADDRESS_LEN);
                break;
            case DB_MESSAGE_RECV:
                evbuffer_add(plain, msg->dbmsg_client->body_recv_id, MESSAGE_ID_LEN);
                break;
        }
        
        rsa_buffer_encrypt(plain, cont->remote_enc_key_pub, phand->buffer, NULL);
        ed25519_buffer_sign(phand->buffer, 0, cont->local_sig_key_priv);

        evbuffer_free(plain);
        db_contact_free(cont);
    }
}

// Free message handler object
static void recv_cleanup(struct prot_recv_handler *phand) {

}

// Handler incomming message
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {

}

// Allocate new message object
static struct prot_message * prot_message_new(sqlite3 *db) {
    struct prot_message *msg;

    msg = safe_malloc(sizeof(struct prot_message),
        "Failed to allocate message protocol handler");
    memset(msg, 0, sizeof(struct prot_message));

    msg->db = db;

    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_MESSAGE_CONTAINER;
    msg->hrecv.require_transaction = 1;
    msg->hrecv.handle_cb = recv_handle;
    msg->hrecv.cleanup_cb = recv_cleanup;

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_MESSAGE_CONTAINER;
    msg->htran.buffer = evbuffer_new();
    msg->htran.done_cb = tran_done;
    msg->htran.setup_cb = tran_setup;
    msg->htran.cleanup_cb = tran_cleanup;
}

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_client_new(sqlite3 *db, enum prot_message_to to, struct db_message *dbmsg) {
    struct prot_message *msg;

    msg = prot_message_new(db);
    msg->to = to;
    msg->dbmsg_client = dbmsg;
}

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_mailbox_new(sqlite3 *db, struct db_mb_message *dbmsg) {
    struct prot_message *msg;

    msg = prot_message_new(db);
    msg->dbmsg_mailbox = dbmsg;
}

void prot_message_free(struct prot_message *msg) {
    if (!msg) return;

    if (msg->dbmsg_client)
        db_message_free(msg->dbmsg_client);
    if (msg->dbmsg_mailbox)
        db_mb_message_free(msg->dbmsg_mailbox);

    evbuffer_free(msg->htran.buffer);
    free(msg);
}