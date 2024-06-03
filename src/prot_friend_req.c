#include <stdlib.h>
#include <onion.h>
#include <db_options.h>
#include <db_contact.h>
#include <sys_memory.h>
#include <prot_main.h>
#include <sqlite3.h>
#include <prot_ack.h>
#include <prot_friend_req.h>
#include <prot_main.h>
#include <buffer_crypto.h>
#include <event2/buffer.h>
#include <debug.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/encoder.h>
#include <helpers_crypto.h>

// Called when ACK message is received (or cleaned up)
static void ack_received_cb(int ack_success, struct prot_main *pmain, void *arg) {
    struct prot_friend_req *msg = arg;

    if (ack_success) {
        db_contact_save(msg->db, msg->friend);
    }

    hook_list_call(pmain->hooks, 
        ack_success ? PROT_FRIEND_REQ_EV_OK : PROT_FRIEND_REQ_EV_FAIL, msg->friend);
    prot_friend_req_free(msg);
}

// Called if friend request is send successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_ack_ed25519 *ack;
    struct prot_friend_req *msg = phand->msg;

    // Will be cleaned up by ack messages
    phand->cleanup_cb = NULL;

    ack = prot_ack_ed25519_new(PROT_ACK_ONION, msg->friend->onion_pub_key, NULL, ack_received_cb, msg);
    prot_main_push_recv(pmain, &(ack->hrecv));
}

// Free friend request object
static void tran_cleanup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_friend_req *msg = phand->msg;

    hook_list_call(pmain->hooks, PROT_FRIEND_REQ_EV_FAIL, msg->friend);
    prot_friend_req_free(msg);
}

// Called to build friend request message and put it into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    uint8_t nick_len;
    char nick[CLIENT_NICK_MAX_LEN + 1];
    struct prot_friend_req *msg = phand->msg;

    char onion_address[ONION_ADDRESS_LEN + 1];      // My onion address
    char mb_onion_address[ONION_ADDRESS_LEN + 1];   // My mailbox address
    uint8_t mb_id[MAILBOX_ID_LEN];              // My mailbox ID
    uint8_t onion_priv_key[ONION_PRIV_KEY_LEN]; // My onion private key (to sign the message)

    // Generate ED25519 keypair
    ed25519_keygen(msg->friend->local_sig_key_pub, msg->friend->local_sig_key_priv);
    // Generate RSA 2048bit keypair
    rsa_2048bit_keygen(msg->friend->local_enc_key_pub, msg->friend->local_enc_key_priv);

    // Fetch data from the database
    db_options_get_text(msg->db, "onion_address", onion_address, ONION_ADDRESS_LEN + 1);
    db_options_get_bin(msg->db, "onion_private_key", onion_priv_key, ONION_PRIV_KEY_LEN);
    nick_len = db_options_get_text(msg->db, "client_nickname", nick, CLIENT_NICK_MAX_LEN);

    debug("Nick len: %d", nick_len);

    // If client has mailbox use mailbox fields, otherwise set them to 0
    if (db_options_is_defined(msg->db, "client_mailbox_id", DB_OPTIONS_BIN)) {
        db_options_get_bin(msg->db, "client_mailbox_id", mb_id, MAILBOX_ID_LEN);
    } else {
        memset(mb_id, 0, sizeof(mb_id));
    }
    if (db_options_is_defined(msg->db, "client_mailbox_onion_address", DB_OPTIONS_TEXT)) {
        db_options_get_text(msg->db, "client_mailbox_onion_address", mb_onion_address, ONION_ADDRESS_LEN + 1);
    } else {
        memset(mb_onion_address, 0, sizeof(mb_onion_address));
    }

    // Stuff all data into buffer
    evbuffer_add(phand->buffer, prot_header(PROT_FRIEND_REQUEST), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
    evbuffer_add(phand->buffer, onion_address, ONION_ADDRESS_LEN);
    evbuffer_add(phand->buffer, msg->friend->local_sig_key_pub, CLIENT_SIG_KEY_PUB_LEN);
    evbuffer_add(phand->buffer, msg->friend->local_enc_key_pub, CLIENT_ENC_KEY_PUB_LEN);

    evbuffer_add(phand->buffer, mb_onion_address, ONION_ADDRESS_LEN);
    evbuffer_add(phand->buffer, mb_id, MAILBOX_ID_LEN);

    evbuffer_add(phand->buffer, &nick_len, sizeof(nick_len));
    evbuffer_add(phand->buffer, nick, nick_len);
    
    // Sign the buffer
    db_contact_onion_extract_key(msg->friend);
    ed25519_buffer_sign(phand->buffer, 0, onion_priv_key);
}

// Free friend request object
static void recv_cleanup(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_friend_req *msg = phand->msg;
    prot_friend_req_free(msg);
}

static void ack_sent_cb(int ack_success, struct prot_main *pmain, void *arg) {
    struct prot_friend_req *msg = arg;

    if (ack_success) {
        db_contact_save(msg->db, msg->friend);
        hook_list_call(pmain->hooks, PROT_FRIEND_REQ_EV_INCOMMING, msg->friend);
    }

    prot_friend_req_free(msg);
}

// Called to handle incomming friend request
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct evbuffer *input;
    struct prot_friend_req *msg = phand->msg;
    struct prot_ack_ed25519 *ack;

    uint8_t onion_priv_key[ONION_PRIV_KEY_LEN];

    uint8_t *onion_addr_ptr;
    uint8_t received_onion_key[ONION_PUB_KEY_LEN];
    char received_onion_address[ONION_ADDRESS_LEN];

    uint8_t nick_len;
    uint8_t *message_static;

    // Length of all fields before nickname field
    int message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN + ONION_ADDRESS_LEN + 
        CLIENT_SIG_KEY_PUB_LEN + CLIENT_ENC_KEY_PUB_LEN + ONION_ADDRESS_LEN + MAILBOX_ID_LEN + 1;

    input = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(input) < message_len)
        return;

    debug("LEN 1 OK");

    message_static = evbuffer_pullup(input, message_len);
    nick_len = message_static[message_len - 1]; // Last byte of static fields
    debug("nick len: %d", nick_len);

    // If nick is too large
    if (nick_len > CLIENT_NICK_MAX_LEN) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG); 
        return;
    }

    debug("NICK LEN OK");

    // Add nickname and signature length
    message_len += nick_len + ED25519_SIGNATURE_LEN;

    // Wait for all data
    if (evbuffer_get_length(input) < message_len)
        return;

    debug("LEN 2 OK");

    onion_addr_ptr = message_static + PROT_HEADER_LEN + TRANSACTION_ID_LEN;

    if (!onion_address_valid(onion_addr_ptr)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    debug("ONION OK");

    onion_extract_key(onion_addr_ptr, received_onion_key);
    if (!ed25519_buffer_validate(input, message_len, received_onion_key)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    debug("BUFFER SIG OK");

    evbuffer_drain(input, PROT_HEADER_LEN + TRANSACTION_ID_LEN);
    evbuffer_remove(input, received_onion_address, ONION_ADDRESS_LEN);

    // Check for this onion on the database
    msg->friend = db_contact_get_by_onion(msg->db, received_onion_address, NULL);
    if (!msg->friend) {
        msg->friend = db_contact_new();
        msg->friend->status = DB_CONTACT_PENDING_IN;
        memcpy(msg->friend->onion_address, received_onion_address, ONION_ADDRESS_LEN);
        
    } else if (msg->friend->status == DB_CONTACT_PENDING_OUT) {
        msg->friend->status = DB_CONTACT_ACTIVE;
    }

    evbuffer_remove(input, msg->friend->remote_sig_key_pub, CLIENT_SIG_KEY_PUB_LEN);
    evbuffer_remove(input, msg->friend->remote_enc_key_pub, CLIENT_ENC_KEY_PUB_LEN);

    evbuffer_remove(input, msg->friend->mailbox_onion, ONION_ADDRESS_LEN);
    evbuffer_remove(input, msg->friend->mailbox_id, MAILBOX_ID_LEN);
    msg->friend->has_mailbox = !!(msg->friend->mailbox_id[0]);

    // Get nickname
    evbuffer_drain(input, 1); // Drain nickname length (we already have it)
    msg->friend->nickname_len = nick_len;
    evbuffer_remove(input, msg->friend->nickname, nick_len);
    // Drain signature
    evbuffer_drain(input, ED25519_SIGNATURE_LEN);

    // Get local user's onion priv key from database
    db_options_get_bin(msg->db, "onion_private_key", onion_priv_key, ONION_PRIV_KEY_LEN);

    // Time to send ACK, cleanup is disabled since ACK will do it
    phand->cleanup_cb = NULL;
    ack = prot_ack_ed25519_new(PROT_ACK_ONION, NULL, onion_priv_key, ack_sent_cb, msg);
    prot_main_push_tran(pmain, &(ack->htran));
    pmain->current_recv_done = 1;

    debug("ALL DONE");
}

// Allocate new friend request handler
struct prot_friend_req * prot_friend_req_new(sqlite3 *db, const char *onion_address) {
    struct prot_friend_req *msg;

    msg = safe_malloc(sizeof(struct prot_friend_req), 
        "Failed to allocate new friend request message");
    memset(msg, 0, sizeof(struct prot_friend_req));

    msg->db = db;

    if (onion_address) {
        msg->friend = db_contact_get_by_onion(db, onion_address, NULL);

        if (!msg->friend) {
            msg->friend = db_contact_new();
            msg->friend->status = DB_CONTACT_PENDING_OUT;
            memcpy(msg->friend->onion_address, onion_address, ONION_ADDRESS_LEN);
        } else {
            msg->friend->status = DB_CONTACT_ACTIVE;
        }
    }

    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_FRIEND_REQUEST;
    msg->hrecv.require_transaction = 1;
    msg->hrecv.handle_cb = recv_handle;
    msg->hrecv.cleanup_cb = recv_cleanup;

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_FRIEND_REQUEST;
    msg->htran.done_cb = tran_done;
    msg->htran.setup_cb = tran_setup;
    msg->htran.cleanup_cb = tran_cleanup;
    msg->htran.buffer = evbuffer_new();

    return msg;
}

// Free memory for given friend request
void prot_friend_req_free(struct prot_friend_req *msg) {
    if (msg && msg->friend)
        db_contact_free(msg->friend);
    if (msg && msg->htran.buffer)
        evbuffer_free(msg->htran.buffer);

    free(msg);
}