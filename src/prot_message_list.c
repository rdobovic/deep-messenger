#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <prot_main.h>
#include <db_message.h>
#include <db_mb_message.h>
#include <prot_message_list.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <buffer_crypto.h>
#include <onion.h>
#include <queue.h>
#include <sys_memory.h>
#include <debug.h>
#include <db_options.h>
#include <array.h>
#include <prot_client_fetch.h>
#include <prot_mb_fetch.h>

// Called when message list is sent successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    int i;
    struct prot_message_list *msg = phand->msg;
    struct prot_message_list_ev_data evdata =
        { msg->n_client_msgs, msg->client_msgs };

    debug("DONE PML messages %d %p %p", msg->n_client_msgs, msg->client_msgs, msg->client_cont);
    
    for (i = 0; i < msg->n_client_msgs; i++) {
        struct db_message *dbmsg = msg->client_msgs[i];

        if (dbmsg->status == DB_MESSAGE_STATUS_UNDELIVERED) {
            dbmsg->status = DB_MESSAGE_STATUS_SENT_CONFIRMED;
            db_message_save(msg->db, dbmsg);
        }
    }

    if (pmain->mode == PROT_MODE_CLIENT) {
        hook_list_call(pmain->hooks, PROT_CLIENT_FETCH_EV_INCOMMING, &evdata);
    }
}

// Called to free memory taken by handler object when transmission is done
static void tran_cleanup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_message_list *msg = phand->msg;
    prot_message_list_free(msg);
}

// Called to serilize message and put it into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_message_list *msg = phand->msg;
    int i;
    uint32_t length = 0;

    debug("Transmission setup PML");

    if (pmain->mode == PROT_MODE_CLIENT) {
        struct db_contact *cont = msg->client_cont;

        for (i = 0; i < msg->n_client_msgs; i++) {
            struct db_message *dbmsg = msg->client_msgs[i];
            struct evbuffer *plain;
            struct evbuffer *encrypted;

            uint8_t ctype = dbmsg->type;

            if (dbmsg->contact_id != cont->id)
                continue;

            plain = evbuffer_new();
            encrypted = evbuffer_new();

            evbuffer_add(encrypted, prot_header(PROT_MESSAGE_CONTAINER), PROT_HEADER_LEN);
            evbuffer_add(encrypted, pmain->transaction_id, TRANSACTION_ID_LEN);
            evbuffer_add(encrypted, cont->mailbox_id, MAILBOX_ID_LEN);
            evbuffer_add(encrypted, cont->local_sig_key_pub, CLIENT_SIG_KEY_PUB_LEN);
            evbuffer_add(encrypted, dbmsg->global_id, MESSAGE_ID_LEN);

            evbuffer_add(plain, &ctype, sizeof(ctype));

            switch (dbmsg->type) {
                case DB_MESSAGE_TEXT:
                    evbuffer_add(plain, dbmsg->body_text, dbmsg->body_text_len);
                    break;
                case DB_MESSAGE_NICK:
                    evbuffer_add(plain, dbmsg->body_nick, dbmsg->body_nick_len);
                    break;
                case DB_MESSAGE_RECV:
                    evbuffer_add(plain, dbmsg->body_recv_id, MESSAGE_ID_LEN);
                    break;
                case DB_MESSAGE_MBOX:
                    evbuffer_add(plain, dbmsg->body_mbox_id, MAILBOX_ID_LEN);
                    evbuffer_add(plain, dbmsg->body_mbox_onion, ONION_ADDRESS_LEN);
                    break;
            }

            rsa_buffer_encrypt(plain, cont->remote_enc_key_pub, encrypted, NULL);
            evbuffer_free(plain);

            ed25519_buffer_sign(encrypted, 0, cont->local_sig_key_priv);
            evbuffer_add_buffer_reference(phand->buffer, encrypted);

            length += evbuffer_get_length(encrypted);
            evbuffer_free(encrypted);
        }

        length = htonl(length);
        evbuffer_prepend(phand->buffer, &length, sizeof(length));
        evbuffer_prepend(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
        evbuffer_prepend(phand->buffer, prot_header(PROT_MESSAGE_LIST), PROT_HEADER_LEN);

        ed25519_buffer_sign(phand->buffer, 0, cont->local_sig_key_priv);

        debug("Transmission setup PML DONE for %d messages %p", msg->n_client_msgs, msg->client_msgs);
    }

    if (pmain->mode == PROT_MODE_MAILBOX) {
        int i;
        uint8_t mb_sig_priv_key[ONION_PRIV_KEY_LEN];
        
        for (i = 0; i < msg->n_mailbox_msgs; i++) {
            length += msg->mailbox_msgs[i]->data_len;
            evbuffer_add(phand->buffer, msg->mailbox_msgs[i]->data, msg->mailbox_msgs[i]->data_len);
        }

        length = htonl(length);
        evbuffer_prepend(phand->buffer, &length, sizeof(length));
        evbuffer_prepend(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
        evbuffer_prepend(phand->buffer, prot_header(PROT_MESSAGE_LIST), PROT_HEADER_LEN);

        db_options_get_bin(msg->db, "onion_private_key", mb_sig_priv_key, ONION_PRIV_KEY_LEN);
        ed25519_buffer_sign(phand->buffer, 0, mb_sig_priv_key);
    }
}

// Called to free memeory taken by the handler object when message is processed
static void recv_cleanup(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_message_list *msg = phand->msg;

    if (!phand->success) {
        if (msg->from == PROT_MESSAGE_LIST_FROM_CLIENT)
            hook_list_call(pmain->hooks, PROT_CLIENT_FETCH_EV_FAIL, NULL);
        if (msg->from == PROT_MESSAGE_LIST_FROM_MAILBOX)
            hook_list_call(pmain->hooks, PROT_MB_FETCH_EV_FAIL, NULL);
    }

    prot_message_list_free(msg);
}

// Called to handle incomming message
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_message_list *msg = phand->msg; // Message handler instance
    int i;
    uint32_t length;                            // List length (size in bytes)
    struct evbuffer *input;                     // Bufferevent input buffer
    struct evbuffer_ptr pos;                    // Buffer position pointer
    uint8_t key[ED25519_PUB_KEY_LEN];           // Key used to check message signature
    struct prot_message_list_ev_data evdata;    // Data passed to hook callback

    // Full message length
    size_t message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN + sizeof(length);

    // Get buffer and check buffer length, wait for entire message to arrive
    input = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(input) < message_len)
        return;

    evbuffer_ptr_set(input, &pos, message_len - sizeof(length), EVBUFFER_PTR_SET);
    evbuffer_copyout_from(input, &pos, &length, sizeof(length));
    length = ntohl(length);

    message_len += length + ED25519_SIGNATURE_LEN;

    if (evbuffer_get_length(input) < message_len)
        return;

    debug("Length OK");

    // If message is from client use client key to verify it, otherwise use
    // mailbox onion key
    if (msg->from == PROT_MESSAGE_LIST_FROM_CLIENT) {
        memcpy(key, msg->client_cont->remote_sig_key_pub, CLIENT_SIG_KEY_PUB_LEN);
    } else {
        char mb_onion[ONION_ADDRESS_LEN + 1];
        db_options_get_text(msg->db, "client_mailbox_onion_address", 
            mb_onion, ONION_ADDRESS_LEN + 1);
        onion_extract_key(mb_onion, key);
    }

    if (!ed25519_buffer_validate(input, message_len, key)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    debug("List signature OK");

    // Remove list header
    evbuffer_drain(input, PROT_HEADER_LEN + TRANSACTION_ID_LEN + sizeof(length));

    evdata.n_messages = 0;
    evdata.messages = array(struct db_message *);
    
    // Process all messages
    while (length > 0) {
        int rc, i;
        uint8_t ctype;                   // Message content type
        uint32_t data_len;               // Encrypted message body length
        size_t plain_len;                // Decrypted message body length
        uint8_t *plain_data;             // Pointer to decrypted message body
        struct evbuffer *plain = NULL;   // Buffer that contains decrypted message body
        struct db_message *dbmsg = NULL; // Message object

        size_t header_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN + MAILBOX_ID_LEN +
            CLIENT_SIG_KEY_PUB_LEN + MESSAGE_ID_LEN + sizeof(data_len);
        size_t message_len = header_len;

        // Global message ID
        uint8_t gid[MESSAGE_ID_LEN];
        // Message sender public signing key
        uint8_t contact_sig_key[CLIENT_SIG_KEY_PUB_LEN];

        if (length < message_len)
            break;

        debug("Got message in the list");

        // Get message encrypted body length
        evbuffer_ptr_set(input, &pos, message_len - sizeof(data_len), EVBUFFER_PTR_SET);
        evbuffer_copyout_from(input, &pos, &data_len, sizeof(data_len));
        data_len = ntohl(data_len);

        // Get message sender signing key
        evbuffer_ptr_set(input, &pos, PROT_HEADER_LEN + TRANSACTION_ID_LEN + 
            MAILBOX_ID_LEN, EVBUFFER_PTR_SET);
        evbuffer_copyout_from(input, &pos, contact_sig_key, CLIENT_SIG_KEY_PUB_LEN);

        message_len += data_len + AES_ENC_KEY_LENGTH + AES_IV_LENGTH + ED25519_SIGNATURE_LEN;

        if (length < message_len)
            break;
        length -= message_len;

        debug("Message len OK %d", message_len);
        
        // Validate buffer signature
        if (!ed25519_buffer_validate(input, message_len, contact_sig_key)) {
            debug("Message sig FAIL");
            goto message_free;
        }

        // Search for the sender in the database
        if (!(msg->client_cont = db_contact_get_by_rsk_pub(msg->db, contact_sig_key, msg->client_cont))) {
            goto message_free;
        }

        debug("Message sig OK");

        message_len -= (header_len - sizeof(data_len));
        evbuffer_drain(input, header_len - sizeof(data_len) - MESSAGE_ID_LEN);
        evbuffer_remove(input, gid, MESSAGE_ID_LEN);

        debug("Message checking existance");
        if (dbmsg = db_message_get_by_gid(msg->db, gid, NULL)) {
            debug("Message exists NOT OK");
            goto message_free;
        }
        debug("Message doesn't exist OK");

        plain = evbuffer_new();
        dbmsg = db_message_new();

        if (rc = rsa_buffer_decrypt(input, msg->client_cont->local_enc_key_priv, plain, NULL)) {
            debug("Failed to decrypt: %d", rc);
            goto message_free;
        }
        debug("Message decrypted");

        evbuffer_remove(plain, &ctype, sizeof(ctype));
        plain_len = evbuffer_get_length(plain);
        plain_data = evbuffer_pullup(plain, plain_len);

        dbmsg->type = ctype;
        dbmsg->sender = DB_MESSAGE_SENDER_FRIEND;
        dbmsg->contact_id = msg->client_cont->id;
        memcpy(dbmsg->global_id, gid, MESSAGE_ID_LEN);

        dbmsg->status = msg->from == PROT_MESSAGE_LIST_FROM_CLIENT ? 
            DB_MESSAGE_STATUS_RECV_CONFIRMED : DB_MESSAGE_STATUS_RECV;

        switch (ctype) {
            case DB_MESSAGE_TEXT:
                debug("Message type is text");
                db_message_set_text(dbmsg, plain_data, plain_len);
                break;
            case DB_MESSAGE_NICK:
                if (plain_len > CLIENT_NICK_MAX_LEN) {
                    goto message_free;
                }
                // Update message
                memcpy(dbmsg->body_nick, plain_data, plain_len);
                dbmsg->body_nick_len = plain_len;
                // Update nickname
                memcpy(msg->client_cont->nickname, plain_data, plain_len);
                msg->client_cont->nickname_len = plain_len;
                msg->client_cont->nickname[plain_len] = '\0';
                break;
            case DB_MESSAGE_MBOX:
                if (plain_len < MAILBOX_ID_LEN + ONION_ADDRESS_LEN) {
                    goto message_free;
                }
                memcpy(dbmsg->body_mbox_id, plain_data, MAILBOX_ID_LEN);
                // Check the onion address
                if (!onion_address_valid(dbmsg->body_mbox_onion)) {
                    goto message_free;
                }
                memcpy(dbmsg->body_mbox_onion, plain_data + MAILBOX_ID_LEN, ONION_ADDRESS_LEN);

                for (i = 0; i < MAILBOX_ID_LEN; i++)
                    if (dbmsg->body_mbox_id[i] != 0)
                        break;

                msg->client_cont->has_mailbox = i < MAILBOX_ID_LEN;
                    
                if (msg->client_cont->has_mailbox) {
                    memcpy(msg->client_cont->mailbox_id, plain_data, MAILBOX_ID_LEN);
                    memcpy(msg->client_cont->mailbox_onion, plain_data + MAILBOX_ID_LEN, ONION_ADDRESS_LEN);
                }
                break;
            case DB_MESSAGE_RECV:
                if (plain_len < MESSAGE_ID_LEN) {
                    goto message_free;
                }
                // Try to find message to be confirmed
                if (!db_message_get_by_gid(msg->db, plain_data, dbmsg)) {
                    goto message_free;
                }
                dbmsg->status = DB_MESSAGE_STATUS_SENT_CONFIRMED;
                break;
            default:
                goto message_free;
        }

        db_message_save(msg->db, dbmsg);
        db_contact_save(msg->db, msg->client_cont);

        // Save message object to event (hook) data
        array_set(evdata.messages, evdata.n_messages, dbmsg);
        ++evdata.n_messages;
        dbmsg = NULL;

        message_free:
        evbuffer_drain(input, message_len);
        if (plain)
            evbuffer_free(plain);
        if (dbmsg)
            db_message_free(dbmsg);
    }

    if (msg->from == PROT_MESSAGE_LIST_FROM_CLIENT) {
        hook_list_call(pmain->hooks, PROT_CLIENT_FETCH_EV_OK, &evdata);
    }
    if (msg->from == PROT_MESSAGE_LIST_FROM_MAILBOX) {
        hook_list_call(pmain->hooks, PROT_MB_FETCH_EV_OK, &evdata);
    }

    for (i = 0; i < evdata.n_messages; i++) {
        db_message_free(evdata.messages[i]);
    }
    array_free(evdata.messages);

    evbuffer_drain(input, length);
    evbuffer_drain(input, ED25519_SIGNATURE_LEN);
    pmain->current_recv_done = 1;
}

// Allocate new handler object
static struct prot_message_list * prot_message_list_new(sqlite3 *db) {
    struct prot_message_list *msg;

    msg = safe_malloc(sizeof(struct prot_message_list),
        "Failed to allocate memory for message list handler");
    memset(msg, 0, sizeof(struct prot_message_list));

    msg->db = db;
    msg->from = PROT_MESSAGE_LIST_FROM_CLIENT;

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_MESSAGE_LIST;
    msg->htran.done_cb = tran_done;
    msg->htran.setup_cb = tran_setup;
    msg->htran.cleanup_cb = tran_cleanup;
    msg->htran.buffer = evbuffer_new();

    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_MESSAGE_LIST;
    msg->hrecv.require_transaction = 1;
    msg->hrecv.handle_cb = recv_handle;
    msg->hrecv.cleanup_cb = recv_cleanup;

    return msg;
}

// Allocate new message list handler (when in the client mode)
struct prot_message_list * prot_message_list_client_new(
    sqlite3 *db, struct db_contact *cont, struct db_message **msgs, int n_msgs
) {
    struct prot_message_list *msg;

    msg = prot_message_list_new(db);
    msg->client_cont = cont;
    msg->client_msgs = msgs;
    msg->n_client_msgs = n_msgs;
    debug("Creating list for %d messages", n_msgs);

    return msg;
}

// Allocate new message list handler (when in the mailbox mode)
struct prot_message_list * prot_message_list_mailbox_new(sqlite3 *db, struct db_mb_message **msgs, int n_msgs) {
    struct prot_message_list *msg;

    msg = prot_message_list_new(db);
    msg->mailbox_msgs = msgs;
    msg->n_mailbox_msgs = n_msgs;

    return msg;
}

// When creating message receive handler use this function to set where is the
// message list comming from, is it from CLIENT or the MAILBOX
void prot_message_list_from(struct prot_message_list *msg, enum prot_message_list_from from) {
    msg->from = from;
}

// Free given message list handler and messages given to new method
void prot_message_list_free(struct prot_message_list *msg) {
    if (!msg) return;
    debug("message list free");

    if (msg->n_mailbox_msgs > 0)
        db_mb_message_free_all(msg->mailbox_msgs, msg->n_mailbox_msgs);

    evbuffer_free(msg->htran.buffer);
    free(msg);
}