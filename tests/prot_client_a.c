#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <sys/stat.h>

#include <helpers_crypto.h>
#include <db_init.h>
#include <debug.h>
#include <constants.h>
#include <prot_main.h>
#include <prot_transaction.h>
#include <prot_friend_req.h>
#include <prot_message.h>
#include <prot_ack.h>
#include <db_message.h>
#include <base32.h>
#include <prot_client_fetch.h>
#include <prot_mb_account.h>
#include <prot_mb_set_contacts.h>
#include <hooks.h>

#define LOCALHOST 0x7F000001

#define CLIENT_A_PORT 5005
#define CLIENT_A_DIR "./tests_data/prot_client_a"

#define CLIENT_A_PORT 5006
#define CLIENT_B_DIR "./tests_data/prot_client_b"

struct prot_main *pmain;

void pmain_done_cb(int ev, void *data, void *cbarg) {
    struct prot_main *pmain = data;

    uint8_t *i = pmain->transaction_id;
    debug("Finished with transaction id: %x%x%x", i[0], i[1], i[2]);
}

void del_status_cb(int ev, void *data, void *cbarg) {
    debug("ACOUNT DELETE status: %s", ev == PROT_MB_ACC_DELETE_EV_OK ? "OK" : "FAIL");
}

void cont_status_cb(int ev, void *data, void *cbarg) {
    struct prot_mb_set_contacts *msg = data;
    struct prot_mb_acc *accdel;

    debug("CONTACTS SET status: %s", ev == PROT_MB_SET_CONTACTS_EV_OK ? "OK" : "FAIL");
    debug("Starting mailbox delete request, press key to continue");
    getchar();

    accdel = prot_mb_acc_delete_new(dbg, msg->onion_address, msg->cl_mb_id, msg->cl_sig_priv_key);
    hook_add(pmain->hooks, PROT_MB_ACC_DELETE_EV_OK, del_status_cb, NULL);
    hook_add(pmain->hooks, PROT_MB_ACC_DELETE_EV_FAIL, del_status_cb, NULL);
    prot_main_push_tran(pmain, &(accdel->htran));
}

void reg_status_cb(int ev, void *data, void *cbarg) {
    int i;
    int n_conts;
    struct db_contact **conts;
    struct prot_mb_set_contacts *acc_cont;
    struct prot_mb_acc_data *acc = data;
    char mb_id[MAILBOX_ID_LEN * 2 + 1];

    debug("Reg status: %s", ev == PROT_MB_ACC_REGISTER_EV_OK ? "OK" : "FAIL");

    if (ev != PROT_MB_ACC_REGISTER_EV_OK) return;

    for (i = 0; i < MAILBOX_ID_LEN; i++) {
        sprintf(&mb_id[i * 2], "%02x", acc->mailbox_id[i]);
    }

    debug("Mailbox ID: %s", mb_id);
    debug("Starting mailbox set contacts, press key to continue");
    getchar();

    conts = db_contact_get_all(dbg, &n_conts);
    debug("Contacts to send: %d", n_conts);
    acc_cont = prot_mb_set_contacts_new(dbg, acc->onion_address, acc->mailbox_id, acc->sig_priv_key, conts, n_conts);
    hook_add(pmain->hooks, PROT_MB_SET_CONTACTS_EV_OK, cont_status_cb, NULL);
    hook_add(pmain->hooks, PROT_MB_SET_CONTACTS_EV_FAIL, cont_status_cb, NULL);
    prot_main_push_tran(pmain, &(acc_cont->htran));
}

int step = 0;

int main() {
    struct prot_txn_req *treq;
    struct prot_friend_req *freq;
    struct prot_message *pmsg;
    struct db_message *dbmsg;
    struct prot_client_fetch *cfet;
    struct db_contact *cont;
    struct prot_mb_acc *acc;

    debug_set_fp(stdout);

    struct event_base *base;
    struct sockaddr_in sin;
    struct bufferevent *bev;

    uint8_t cl_a_onion_pub_key[ONION_PUB_KEY_LEN];
    uint8_t cl_a_onion_priv_key[ONION_PRIV_KEY_LEN];

    char cl_a_onion_address[ONION_ADDRESS_LEN + 2];
    char cl_b_onion_address[ONION_ADDRESS_LEN + 2];

    FILE *hostname_fp;

    struct stat st = {0};

    base = event_base_new();

    // Created data directory
    if (stat(CLIENT_A_DIR, &st) == -1) {
        mkdir(CLIENT_A_DIR, 0751);
        mkdir(CLIENT_A_DIR "/onion", 0700);
    }

    ed25519_keygen(cl_a_onion_pub_key, cl_a_onion_priv_key);

    if (!onion_init_dir(CLIENT_A_DIR "/onion", cl_a_onion_pub_key, cl_a_onion_priv_key)) {
        debug("Failed to create client directory");
        return 0;
    }

    db_init_global(CLIENT_A_DIR "/database.db");
    db_init_schema(dbg);

    if (!(hostname_fp = fopen(CLIENT_A_DIR "/onion/hostname", "r"))) {
        debug("Failed to open client A hostname");
        return 0;
    }
    fread(cl_a_onion_address, 1, ONION_ADDRESS_LEN, hostname_fp);
    cl_a_onion_address[ONION_ADDRESS_LEN] = '\0';
    fclose(hostname_fp);

    if (!(hostname_fp = fopen(CLIENT_B_DIR "/onion/hostname", "r"))) {
        debug("Failed to open client B hostname");
        return 0;
    }
    fread(cl_b_onion_address, 1, ONION_ADDRESS_LEN, hostname_fp);
    cl_b_onion_address[ONION_ADDRESS_LEN] = '\0';
    fclose(hostname_fp);

    





    return 0;


    memset(&sin, 0, sizeof(sin));
    sin.sin_port = htons(DEEP_MESSENGER_PORT);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(LOCALHOST);

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

    if (bufferevent_socket_connect(bev, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        debug("Connection failed");
        return 1;
    }

    pmain = prot_main_new(base, dbg);
    prot_main_assign(pmain, bev);
    hook_add(pmain->hooks, PROT_MAIN_EV_DONE, pmain_done_cb, NULL);

    treq = prot_txn_req_new();
    prot_main_push_tran(pmain, &(treq->htran));

    //freq = prot_friend_req_new(dbg, "scddeoyfwo2nbioc55fd5hys4voln4qbm3xu65ffhhvzl6rnv7h2aeid.onion");
    freq = prot_friend_req_new(dbg, "g7kfkvigtyx45az27obwydfq3zrxfwl77so3n3tqv22cw3qvz6cuv4qd.onion");
    //prot_main_push_tran(pmain, &(freq->htran));

    dbmsg = db_message_new();
    dbmsg->type = DB_MESSAGE_TEXT;
    dbmsg->contact_id = 19;
    db_message_gen_id(dbmsg);
    db_message_set_text(dbmsg, "Hi how are you feeling ??", 25);
    //memcpy(dbmsg->body_nick, "rokica", 6);
    //dbmsg->body_nick_len = 6;
    dbmsg->status = DB_MESSAGE_STATUS_UNDELIVERED;
    //db_message_save(dbg, dbmsg);

    pmsg = prot_message_to_client_new(dbg, dbmsg);
    //prot_main_push_tran(pmain, &(pmsg->htran));

    cont = db_contact_get_by_pk(dbg, 19, NULL);
    cfet = prot_client_fetch_new(dbg, cont);
    //prot_main_push_tran(pmain, &(cfet->htran));
    
    debug("Creating new MB register");
    //acc = prot_mb_acc_register_new(dbg, "g7kfkvigtyx45az27obwydfq3zrxfwl77so3n3tqv22cw3qvz6cuv4qd.onion", mb_access_key);
    debug("Adding hooks");
    hook_add(pmain->hooks, PROT_MB_ACC_REGISTER_EV_OK, reg_status_cb, NULL);
    hook_add(pmain->hooks, PROT_MB_ACC_REGISTER_EV_FAIL, reg_status_cb, NULL);
    prot_main_push_tran(pmain, &(acc->htran));

    debug("init end");

    event_base_dispatch(base);
    return 0;
}