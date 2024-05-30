#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <hooks.h>
#include <base32.h>
#include <db_init.h>
#include <debug.h>
#include <constants.h>
#include <db_message.h>
#include <db_options.h>

#include <prot_main.h>
#include <prot_transaction.h>
#include <prot_friend_req.h>
#include <prot_message.h>
#include <prot_ack.h>
#include <prot_client_fetch.h>
#include <prot_mb_account.h>
#include <prot_mb_set_contacts.h>

#define DESTINATION_PORT "5000"

#define MAILBOX_ONION_ADDRESS "g7kfkvigtyx45az27obwydfq3zrxfwl77so3n3tqv22cw3qvz6cuv4qd.onion"
#define MAILBOX_ACCESS_KEY    "xmaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaf3"

struct event_base *base;
struct prot_main *pmain;

void init_operation();

void mb_reg_status(int ev, void *data, void *cbarg) {
    int i;
    struct prot_mb_acc_data *acc = data;
    char mb_id[MAILBOX_ID_LEN * 2 + 1];

    debug("Mailbox registration status: %s", ev == PROT_MB_ACC_REGISTER_EV_OK ? "OK" : "FAIL");
    if (ev != PROT_MB_ACC_REGISTER_EV_OK) return;

    for (i = 0; i < MAILBOX_ID_LEN; i++) {
        sprintf(&mb_id[i * 2], "%02x", acc->mailbox_id[i]);
    }
    debug("Mailbox ID: %s", mb_id);

    db_options_set_bin(dbg, "client_mailbox_id", acc->mailbox_id, MAILBOX_ID_LEN);
    db_options_set_bin(dbg, "client_mailbox_sig_pub_key", acc->sig_pub_key, MAILBOX_ACCOUNT_KEY_PUB_LEN);
    db_options_set_bin(dbg, "client_mailbox_sig_priv_key", acc->sig_priv_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);
    db_options_set_text(dbg, "client_mailbox_onion_address", acc->onion_address, ONION_ADDRESS_LEN);
}

void mb_del_status(int ev, void *data, void *cbarg) {
    int i;
    struct prot_mb_acc_data *acc = data;

    debug("Mailbox account delete status: %s", ev == PROT_MB_ACC_DELETE_EV_OK ? "OK" : "FAIL");
    if (ev != PROT_MB_ACC_DELETE_EV_OK) return;

    db_options_set_bin(dbg, "client_mailbox_id", NULL, 0);
    db_options_set_bin(dbg, "client_mailbox_sig_pub_key", NULL, 0);
    db_options_set_bin(dbg, "client_mailbox_sig_priv_key", NULL, 0);
    db_options_set_text(dbg, "client_mailbox_onion_address", NULL, 0);
}

void mb_set_conts_status(int ev, void *data, void *cbarg) {
    int i;
    struct prot_mb_acc_data *acc = data;

    debug("Mailbox account set contacts status: %s", ev == PROT_MB_SET_CONTACTS_EV_OK ? "OK" : "FAIL");
}

void pmain_done_event(int ev, void *data, void *cbarg) {
    struct prot_main *pmain = data;

    uint8_t *i = pmain->transaction_id;
    debug("Finished with transaction id: %02x%02x...%02x%02x", i[0], i[1], i[14], i[15]);

    prot_main_defer_free(pmain);
    init_operation();
}

void init_operation() {
    int op;
    int rc;
    struct bufferevent *bev;
    struct addrinfo hints;
    struct addrinfo *servinfo, *aip;

    struct prot_txn_req *treq;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    debug("Enter operation number: ");
    scanf("%d", &op);

    if (op == 0) {
        return;
    }

    // List all contacts from the database
    if (op == 5) {
        int i, n_conts;
        struct db_contact **conts;

        debug("List of all contacts in the database:");
        conts = db_contact_get_all(dbg, &n_conts);
        for (i = 0; i < n_conts; i++) {
            debug("%d %s %s", conts[i]->id, conts[i]->nickname, 
                conts[i]->status == DB_CONTACT_PENDING_IN ? "INCOMMING REQ" : 
                conts[i]->status == DB_CONTACT_PENDING_IN ? "OUTGOING REQ" : "ACTIVE");
        }
        db_contact_free_all(conts, n_conts);

        debug("Total: %d", n_conts);
        init_operation();
        return;
    }

    if (rc = getaddrinfo("127.0.0.1", DESTINATION_PORT, &hints, &servinfo)) {
        debug("getaddrinfo: %s", gai_strerror(rc));
        exit(1);
    }

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

    /*for (aip = servinfo; aip != NULL; aip = aip->ai_next) {
        if (bufferevent_socket_connect(bev, aip->ai_addr, aip->ai_addrlen) < 0)
            continue;

        debug("Connected successfully");
        break;
    }

    if (aip == NULL) {
        debug("Failed to connect");
        exit(1);
    }*/

    pmain = prot_main_new(base, dbg);

    prot_main_connect(pmain, MAILBOX_ONION_ADDRESS, servinfo->ai_addr, servinfo->ai_addrlen);

    //prot_main_assign(pmain, bev);
    hook_add(pmain->hooks, PROT_MAIN_EV_DONE, pmain_done_event, NULL);

    treq = prot_txn_req_new();
    prot_main_push_tran(pmain, &(treq->htran));

    // Mailbox register
    if (op == 1) {
        struct prot_mb_acc *acc_reg;
        uint8_t access_key[MAILBOX_ACCESS_KEY_LEN];

        base32_decode(MAILBOX_ACCESS_KEY, strlen(MAILBOX_ACCESS_KEY), access_key);
        acc_reg = prot_mb_acc_register_new(dbg, MAILBOX_ONION_ADDRESS, access_key);
        hook_add(pmain->hooks, PROT_MB_ACC_REGISTER_EV_OK, mb_reg_status, NULL);
        hook_add(pmain->hooks, PROT_MB_ACC_REGISTER_EV_FAIL, mb_reg_status, NULL);
        prot_main_push_tran(pmain, &(acc_reg->htran));
    }

    // Mailbox delete account
    if (op == 2) {
        struct prot_mb_acc *acc_del;
        uint8_t mb_id[MAILBOX_ID_LEN];
        uint8_t mb_onion[ONION_ADDRESS_LEN + 1];
        uint8_t mb_sig_priv_key[MAILBOX_ACCOUNT_KEY_PRIV_LEN];

        db_options_get_bin(dbg, "client_mailbox_id", mb_id, MAILBOX_ID_LEN);
        db_options_get_bin(dbg, "client_mailbox_sig_priv_key", mb_sig_priv_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);
        db_options_get_text(dbg, "client_mailbox_onion_address", mb_onion, ONION_ADDRESS_LEN + 1);

        acc_del = prot_mb_acc_delete_new(dbg, mb_onion, mb_id, mb_sig_priv_key);
        hook_add(pmain->hooks, PROT_MB_ACC_DELETE_EV_OK, mb_del_status, NULL);
        hook_add(pmain->hooks, PROT_MB_ACC_DELETE_EV_FAIL, mb_del_status, NULL);
        prot_main_push_tran(pmain, &(acc_del->htran));
    }

    // Mailbox set contacts
    if (op == 3) {
        int n_conts;
        struct db_contact **conts;
        struct prot_mb_set_contacts *acc_set_conts;
        uint8_t mb_id[MAILBOX_ID_LEN];
        uint8_t mb_onion[ONION_ADDRESS_LEN + 1];
        uint8_t mb_sig_priv_key[MAILBOX_ACCOUNT_KEY_PRIV_LEN];

        db_options_get_bin(dbg, "client_mailbox_id", mb_id, MAILBOX_ID_LEN);
        db_options_get_bin(dbg, "client_mailbox_sig_priv_key", mb_sig_priv_key, MAILBOX_ACCOUNT_KEY_PRIV_LEN);
        db_options_get_text(dbg, "client_mailbox_onion_address", mb_onion, ONION_ADDRESS_LEN + 1);

        conts = db_contact_get_all(dbg, &n_conts);
        debug(">> Uploading %d contacts to the server <<", n_conts);

        acc_set_conts = prot_mb_set_contacts_new(dbg, mb_onion, mb_id, mb_sig_priv_key, conts, n_conts);
        hook_add(pmain->hooks, PROT_MB_SET_CONTACTS_EV_OK, mb_set_conts_status, NULL);
        hook_add(pmain->hooks, PROT_MB_SET_CONTACTS_EV_FAIL, mb_set_conts_status, NULL);
        prot_main_push_tran(pmain, &(acc_set_conts->htran));
    }

    // Friend request
    if (op == 4) {
        struct prot_friend_req *freq;
        char onion_address[ONION_ADDRESS_LEN + 1];

        debug("Enter client onion address: ");
        fgets(onion_address, ONION_ADDRESS_LEN + 1, stdin);
        onion_address[ONION_ADDRESS_LEN] = '\0';

        freq = prot_friend_req_new(dbg, onion_address);
        
    }
}

int main() {

    debug_set_fp(stdout);
    base = event_base_new();

    db_init_global("prot_client_v2.db");
    db_init_schema(dbg);

    init_operation();

    event_base_dispatch(base);
    return 0;
}

