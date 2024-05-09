#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

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

#define LOCALHOST 0x7F000001

void pmain_done_cb(struct prot_main *pmain, void *attr) {
    uint8_t *i = pmain->transaction_id;
    debug("Finished with transaction id: %x%x%x", i[0], i[1], i[2]);
}

void ack_cb(int succ, void *arg) {
    debug("TRANSMITTED ACK: %s", succ ? "OK" : "INVALID");
}

int main() {
    debug_set_fp(stdout);
    struct event_base *base;
    struct sockaddr_in sin;
    struct bufferevent *bev;
    struct prot_main *pmain;
    struct prot_txn_req *treq;
    struct prot_friend_req *freq;
    struct prot_message *pmsg;
    struct db_message *dbmsg;

    base = event_base_new();

    memset(&sin, 0, sizeof(sin));
    sin.sin_port = htons(DEEP_MESSENGER_PORT);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(LOCALHOST);

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

    if (bufferevent_socket_connect(bev, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        debug("Connection failed");
        return 1;
    }

    db_init_global("deep_messenger.db");

    pmain = prot_main_new(base, dbg);
    prot_main_assign(pmain, bev);
    prot_main_setcb(pmain, pmain_done_cb, NULL, NULL);

    treq = prot_txn_req_new();
    prot_main_push_tran(pmain, &(treq->htran));

    //freq = prot_friend_req_new(dbg, "scddeoyfwo2nbioc55fd5hys4voln4qbm3xu65ffhhvzl6rnv7h2aeid.onion");
    freq = prot_friend_req_new(dbg, "g7kfkvigtyx45az27obwydfq3zrxfwl77so3n3tqv22cw3qvz6cuv4qd.onion");
    //prot_main_push_tran(pmain, &(freq->htran));

    dbmsg = db_message_new();
    dbmsg->type = DB_MESSAGE_NICK;
    dbmsg->contact_id = 19;
    db_message_gen_id(dbmsg);
    //db_message_set_text(dbmsg, "This is test message", 20);
    memcpy(dbmsg->body_nick, "rokica", 6);
    dbmsg->body_nick_len = 6;
    dbmsg->status = DB_MESSAGE_STATUS_UNDELIVERED;
    db_message_save(dbg, dbmsg);

    pmsg = prot_message_client_new(dbg, PROT_MESSAGE_TO_CLIENT, dbmsg);
    prot_main_push_tran(pmain, &(pmsg->htran));

    debug("init end");

    event_base_dispatch(base);
    return 0;
}