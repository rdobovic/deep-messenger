#include <stdint.h>
#include <string.h>
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
#include <debug.h>

// Called when ACK is arrived or failed to arrive
static void ack_received_cb(int ack_success, struct prot_main *pmain, void *arg) {
    struct prot_message *msg = arg;

    // When ACK is received and this is client set message as sent
    if (ack_success && pmain->mode == PROT_MODE_CLIENT) {
        msg->client_msg->status = DB_MESSAGE_STATUS_SENT;
        db_message_save(msg->db, msg->client_msg);
    }

    prot_message_free(msg);
}

// Called if message is sent successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_message *msg = phand->msg;
    struct prot_ack_ed25519 *ack;

    // Only client can sent a message outside the message list
    if (pmain->mode == PROT_MODE_CLIENT) {

        if (msg->to == PROT_MESSAGE_TO_CLIENT) {
            ack = prot_ack_ed25519_new(PROT_ACK_SIGNATURE, msg->client_cont->remote_sig_key_pub, NULL, ack_received_cb, msg);
        }

        if (msg->to == PROT_MESSAGE_TO_MAILBOX) {
            uint8_t onion_key[ONION_PUB_KEY_LEN];

            onion_extract_key(msg->client_cont->mailbox_onion, onion_key);
            ack = prot_ack_ed25519_new(PROT_ACK_ONION, onion_key, NULL, ack_received_cb, msg);
        }

        prot_main_push_recv(pmain, &(ack->hrecv));
        phand->cleanup_cb = NULL;
    }
}

// Free message handler object
static void tran_cleanup(struct prot_tran_handler *phand) {
    struct prot_message *msg = phand->msg;
    prot_message_free(msg);
}

// Called to put message into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_message *msg = phand->msg;

    debug("Running msg tran");

    // Only client can send a message outside the message list
    if (pmain->mode == PROT_MODE_CLIENT) {
        uint8_t ctype;
        struct evbuffer *plain;

        plain = evbuffer_new();

        evbuffer_add(phand->buffer, prot_header(PROT_MESSAGE_CONTAINER), PROT_HEADER_LEN);
        evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
        evbuffer_add(phand->buffer, msg->client_cont->mailbox_id, MAILBOX_ID_LEN);
        evbuffer_add(phand->buffer, msg->client_cont->local_sig_key_pub, CLIENT_SIG_KEY_PUB_LEN);
        evbuffer_add(phand->buffer, msg->client_msg->global_id, MESSAGE_ID_LEN);

        ctype = msg->client_msg->type;
        evbuffer_add(plain, &ctype, sizeof(ctype));

        msg->client_msg->sender = DB_MESSAGE_SENDER_ME;

        switch (ctype) {
            case DB_MESSAGE_TEXT:
                evbuffer_add(plain, msg->client_msg->body_text, msg->client_msg->body_text_len);
                break;
            case DB_MESSAGE_NICK:
                evbuffer_add(plain, msg->client_msg->body_nick, msg->client_msg->body_nick_len);
                break;
            case DB_MESSAGE_MBOX:
                evbuffer_add(plain, msg->client_msg->body_mbox_id, MAILBOX_ID_LEN);
                evbuffer_add(plain, msg->client_msg->body_mbox_onion, ONION_ADDRESS_LEN);
                break;
            case DB_MESSAGE_RECV:
                evbuffer_add(plain, msg->client_msg->body_recv_id, MESSAGE_ID_LEN);
                break;
        }
        debug("Created with len (before enc) (%d)", evbuffer_get_length(phand->buffer));
        
        debug("ENC STATUS: %d", rsa_buffer_encrypt(plain, msg->client_cont->remote_enc_key_pub, phand->buffer, NULL));
        ed25519_buffer_sign(phand->buffer, 0, msg->client_cont->local_sig_key_priv);

        debug("Created with len (%d)", evbuffer_get_length(phand->buffer));

        evbuffer_free(plain);
    }
}

// Called when ACK is sent successfully or the sending failed
static void ack_sent_cb(int ack_success, struct prot_main *pmain, void *arg) {
    struct prot_message *msg = arg;

    if (msg->client_msg)
        db_message_save(msg->db, msg->client_msg);
    if (msg->client_cont)
        db_contact_save(msg->db, msg->client_cont);

    if (msg->mailbox_msg)
        db_mb_message_save(msg->db, msg->mailbox_msg);

    prot_message_free(msg);
}

// Free message handler object
static void recv_cleanup(struct prot_recv_handler *phand) {
    struct prot_message *msg = phand->msg;
    prot_message_free(msg);
}

// Handler incomming message
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    int rc;
    uint32_t data_len;
    struct prot_message *msg = phand->msg;
    struct evbuffer *input;
    struct evbuffer_ptr pos;
    struct prot_ack_ed25519 *ack;

    uint8_t mailbox_id[MAILBOX_ID_LEN];
    uint8_t signing_pub_key[CLIENT_SIG_KEY_PUB_LEN];
    uint8_t message_gid[MESSAGE_ID_LEN];

    size_t message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN + MAILBOX_ID_LEN + CLIENT_SIG_KEY_PUB_LEN
        + MESSAGE_ID_LEN + sizeof(data_len);

    input = bufferevent_get_input(pmain->bev);
    debug("Running message handle len(%d) expected min(%d)", evbuffer_get_length(input), message_len);
    if (evbuffer_get_length(input) < message_len)
        return;

    // Get data length from the buffer
    evbuffer_ptr_set(input, &pos, message_len - sizeof(data_len), EVBUFFER_PTR_SET);
    evbuffer_copyout_from(input, &pos, &data_len, sizeof(data_len));
    data_len = ntohl(data_len);

    debug("Data len %d", data_len);

    message_len += data_len + ED25519_SIGNATURE_LEN + AES_IV_LENGTH + AES_ENC_KEY_LENGTH;

    debug("Running message handle len(%d) expected min(%d)", evbuffer_get_length(input), message_len);
    if (evbuffer_get_length(input) < message_len)
        return;

    evbuffer_ptr_set(input, &pos, PROT_HEADER_LEN + TRANSACTION_ID_LEN + MAILBOX_ID_LEN, EVBUFFER_PTR_SET);
    evbuffer_copyout_from(input, &pos, signing_pub_key, CLIENT_SIG_KEY_PUB_LEN);

    if (!ed25519_buffer_validate(input, message_len, signing_pub_key)) {
        debug("Signature invalid key %02x", signing_pub_key[0]);
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    debug("Buffer valid");

    evbuffer_drain(input, PROT_HEADER_LEN);
    evbuffer_drain(input, TRANSACTION_ID_LEN);
    evbuffer_remove(input, mailbox_id, MAILBOX_ID_LEN);
    evbuffer_drain(input, CLIENT_SIG_KEY_PUB_LEN);
    evbuffer_remove(input, message_gid, MESSAGE_ID_LEN);

    debug("Drained some stuff len(%d)", evbuffer_get_length(input));

    if (pmain->mode == PROT_MODE_CLIENT) {
        int i;
        uint8_t ctype;
        size_t plain_len;
        uint8_t *plain_data;
        struct evbuffer *plain = NULL;

        debug("Working as a client");

        if(!(msg->client_cont = db_contact_get_by_rsk_pub(msg->db, signing_pub_key, NULL))) {
            debug("Invalid client");
            prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
            goto err;
        }

        debug("Found the contact");

        // If this message is already here skip processing
        msg->client_msg = db_message_get_by_gid(msg->db, message_gid, NULL);
        if (msg->client_msg)
            goto ack_send;

        msg->client_msg = db_message_new();
        msg->client_msg->contact_id = msg->client_cont->id;
        msg->client_msg->sender = DB_MESSAGE_SENDER_FRIEND;
        memcpy(msg->client_msg->global_id, message_gid, MESSAGE_ID_LEN);

        plain = evbuffer_new();
        if (rc = rsa_buffer_decrypt(input, msg->client_cont->local_enc_key_priv, plain, NULL)) {
            debug("Failed to decrypt: %d", rc);
            prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
            goto err;
        }

        evbuffer_remove(plain, &ctype, sizeof(ctype));
        plain_len = evbuffer_get_length(plain);
        plain_data = evbuffer_pullup(plain, plain_len);

        switch (ctype) {
            case DB_MESSAGE_TEXT:
                db_message_set_text(msg->client_msg, plain_data, plain_len);
                break;
            case DB_MESSAGE_NICK:
                if (plain_len > CLIENT_NICK_MAX_LEN) {
                    prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
                    goto err;
                }
                // Update message
                memcpy(msg->client_msg->body_nick, plain_data, plain_len);
                msg->client_msg->body_nick_len = plain_len;

                memcpy(msg->client_cont->nickname, plain_data, plain_len);
                msg->client_cont->nickname_len = plain_len;
                break;
            case DB_MESSAGE_MBOX:
                if (plain_len < MAILBOX_ID_LEN + ONION_ADDRESS_LEN) {
                    prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
                    goto err;
                }
                memcpy(msg->client_msg->body_mbox_id, plain_data, MAILBOX_ID_LEN);
                memcpy(msg->client_msg->body_mbox_onion, plain_data + MAILBOX_ID_LEN, ONION_ADDRESS_LEN);

                for (i = 0; i < MAILBOX_ID_LEN; i++)
                    if (msg->client_msg->body_mbox_id[i] != 0)
                        break;

                msg->client_cont->has_mailbox = i < MAILBOX_ID_LEN;
                    
                if (msg->client_cont->has_mailbox) {
                    memcpy(msg->client_cont->mailbox_id, plain_data, MAILBOX_ID_LEN);
                    memcpy(msg->client_cont->mailbox_onion, plain_data + MAILBOX_ID_LEN, ONION_ADDRESS_LEN);
                }
                break;
            case DB_MESSAGE_RECV:
                if (plain_len < MESSAGE_ID_LEN) {
                    prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
                    goto err;
                }
                // Try to find message to be confirmed
                if (!db_message_get_by_gid(msg->db, plain_data, msg->client_msg)) {
                    prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
                    goto err;
                }
                msg->client_msg->status = DB_MESSAGE_STATUS_SENT_CONFIRMED;
                break;
            default:
                prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
                goto err;
        }

        msg->client_msg->type = ctype;

        ack_send:
        msg->client_msg->status = DB_MESSAGE_STATUS_RECV;
        ack = prot_ack_ed25519_new(PROT_ACK_SIGNATURE, NULL, msg->client_cont->local_sig_key_priv, ack_sent_cb, msg);
        prot_main_push_tran(pmain, &(ack->htran));
        msg->hrecv.cleanup_cb = NULL;
        pmain->current_recv_done = 1;

        debug("Done, input is len = %d", evbuffer_get_length(input));

        err:
        debug("Err label");
        if (plain)
            evbuffer_free(plain);
        debug("Draining again");
        evbuffer_drain(input, sizeof(data_len) + data_len + AES_ENC_KEY_LENGTH + AES_IV_LENGTH + ED25519_SIGNATURE_LEN);
        debug("Drained len = %d", evbuffer_get_length(input));
        return;
    }

    if (pmain->mode == PROT_MODE_MAILBOX) {
        // Handle message arrival when you are mailbox...
        return;
    }
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
    msg->client_msg = dbmsg;

    if (dbmsg)
        msg->client_cont = db_contact_get_by_pk(db, dbmsg->contact_id, NULL);

    debug("Creating new message container obj %p", &(msg->htran));
    return msg;
}

// Allocate new object for message handler (actual text message)
struct prot_message * prot_message_mailbox_new(sqlite3 *db, struct db_mb_message *dbmsg) {
    struct prot_message *msg;

    msg = prot_message_new(db);
    msg->mailbox_msg = dbmsg;
    return msg;
}

void prot_message_free(struct prot_message *msg) {
    if (!msg) return;

    if (msg->client_msg)
        db_message_free(msg->client_msg);
    if (msg->client_cont)
        db_contact_free(msg->client_cont);
    if (msg->mailbox_msg)
        db_mb_message_free(msg->mailbox_msg);

    evbuffer_free(msg->htran.buffer);
    free(msg);
}