#include <stdint.h>
#include <hooks.h>
#include <onion.h>
#include <helpers_crypto.h>
#include <prot_main.h>
#include <db_mb_key.h>
#include <db_mb_account.h>
#include <prot_mb_account.h>
#include <sys_memory.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <openssl/rand.h>
#include <debug.h>

// Called if request is sent successfully
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    struct prot_mb_acc *acc_granted;

    acc_granted = prot_mb_acc_granted_new(acc);
    prot_main_push_recv(pmain, &(acc_granted->hrecv));

    // Hooks and account data will be freed by acc granted handler
    acc->hooks = NULL;
    acc->cl_acc = NULL;
    acc->success = 1;
}

// Free message handler object
static void tran_cleanup(struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;

    if (!acc->success)
        hook_list_call(acc->hooks, PROT_MB_ACCOUNT_EV_FAIL, acc->cl_acc);

    prot_mb_acc_register_free(acc);
}

// Called to put message into buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;

    evbuffer_add(phand->buffer, prot_header(PROT_MAILBOX_REGISTER), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
    evbuffer_add(phand->buffer, acc->cl_acc->access_key, MAILBOX_ACCESS_KEY_LEN);
    evbuffer_add(phand->buffer, acc->cl_acc->sig_pub_key, MAILBOX_ACCOUNT_KEY_PUB_LEN);
}

// Free message handler object
static void recv_cleanup(struct prot_recv_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    prot_mb_acc_register_free(acc);
}

// Handler incomming message
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct prot_mb_acc *acc = phand->msg;
    struct evbuffer *input;
    struct prot_mb_acc *acc_granted;

    struct db_mb_key *dbkey;
    uint8_t access_key[MAILBOX_ACCESS_KEY_LEN];

    size_t message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN +
        MAILBOX_ACCESS_KEY_LEN + MAILBOX_ACCOUNT_KEY_PUB_LEN;

    input = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(input) < message_len)
        return;
    
    evbuffer_drain(input, PROT_HEADER_LEN + TRANSACTION_ID_LEN);
    evbuffer_remove(input, access_key, MAILBOX_ACCESS_KEY_LEN);

    dbkey = db_mb_key_get_by_key(acc->db, access_key, NULL);

    if (!dbkey || dbkey->uses_left <= 0) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        db_mb_key_free(dbkey);
        return;
    }

    --dbkey->uses_left;
    db_mb_key_save(acc->db, dbkey);
    db_mb_key_free(dbkey);

    acc->mb_acc = db_mb_account_new();
    evbuffer_remove(input, acc->mb_acc->signing_pub_key, MAILBOX_ACCOUNT_KEY_PUB_LEN);

    // Generate random mailbox ID
    RAND_bytes(acc->mb_acc->mailbox_id, MAILBOX_ID_LEN);

    acc_granted = prot_mb_acc_granted_new(acc);
    prot_main_push_tran(pmain, &(acc_granted->htran));
    // Will be freed by granted
    acc->hooks = NULL;
    acc->mb_acc = NULL;

    pmain->current_recv_done = 1;
}

// Create new mailbox register handler
struct prot_mb_acc * prot_mb_acc_register_new(sqlite3 *db, const char *onion_address, const uint8_t *access_key) {
    struct prot_mb_acc *msg;

    msg = safe_malloc(sizeof(struct prot_mb_acc),
        "Failed to allocate mailbox register handler");
    memset(msg, 0, sizeof(struct prot_mb_acc));

    debug("Creating new MB register handler");

    msg->db = db;
    msg->hooks = hook_list_new();

    debug("Creating new MB register handler HOOKS");

    if (onion_address && access_key) {
        msg->cl_acc = safe_malloc(sizeof(struct prot_mb_acc_data), 
            "Failed to allocate memory for prot_acc_data");
        
        memcpy(msg->cl_acc->access_key, access_key, MAILBOX_ACCESS_KEY_LEN);
        memcpy(msg->cl_acc->onion_address, onion_address, ONION_ADDRESS_LEN);

        debug("Creating new MB register handler CL ACC");

        onion_extract_key(onion_address, msg->cl_acc->onion_key);
        debug("Creating new MB register handler EXTRACT");
        ed25519_keygen(msg->cl_acc->sig_pub_key, msg->cl_acc->sig_priv_key);
        debug("Creating new MB register handler KEYGEN");
    }

    debug("Creating new MB register handler HALF");

    msg->htran.msg = msg;
    msg->htran.msg_code = PROT_MAILBOX_REGISTER;
    msg->htran.done_cb = tran_done;
    msg->htran.setup_cb = tran_setup;
    msg->htran.cleanup_cb = tran_cleanup;
    msg->htran.buffer = evbuffer_new();

    msg->hrecv.msg = msg;
    msg->hrecv.msg_code = PROT_MAILBOX_REGISTER;
    msg->hrecv.require_transaction = 1;
    msg->hrecv.handle_cb = recv_handle;
    msg->hrecv.cleanup_cb = recv_cleanup;

    debug("Creating new MB register handler DONE");

    return msg;
}

// Free given mailbox register handler
void prot_mb_acc_register_free(struct prot_mb_acc *msg) {
    if (!msg) return;

    if (msg->hooks)
        hook_list_free(msg->hooks);
    if (msg->cl_acc)
        free(msg->cl_acc);
    if (msg->mb_acc)
        db_mb_account_free(msg->mb_acc);
    
    evbuffer_free(msg->htran.buffer);
    free(msg);
}
