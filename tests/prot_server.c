#include <stdlib.h>
#include <stdint.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#include <debug.h>
#include <constants.h>
#include <prot_main.h>
#include <prot_transaction.h>
#include <prot_ack.h>
#include <base32.h>

#define PUB_KEY "zdqu4sgyyylrjfkvznjyla542xzfpwhy2lla2ve577d5ohsbfnza"

void pmain_done_cb(struct prot_main *pmain, void *attr)
{
    uint8_t *i = pmain->transaction_id;
    debug("Finished with transaction id: %x%x%x", i[0], i[1], i[2]);
}

void ack_cb(int succ, void *arg) {
    debug("RECEIVED ACK: %s", succ ? "OK" : "INVALID");
}

void connect_cb(struct evconnlistener *listener, evutil_socket_t sock,
                struct sockaddr *addr, int len, void *ptr)
{
    struct evbuffer *buff;
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(base, sock, BEV_OPT_CLOSE_ON_FREE);
    struct prot_main *pmain = prot_main_new(base);

    struct prot_txn_req *txn;
    struct prot_ack_ed25519 *ack;
    uint8_t pub_key[ED25519_PUB_KEY_LEN];

    prot_main_setcb(pmain, pmain_done_cb, NULL, NULL);

    base32_decode(PUB_KEY, strlen(PUB_KEY), pub_key);
    txn = prot_txn_req_new();
    ack = prot_ack_ed25519_new(PROT_ACK_SIGNATURE, pub_key, NULL, ack_cb, NULL);

    prot_main_push_recv(pmain, &(txn->hrecv));
    prot_main_push_recv(pmain, &(ack->hrecv));

    buff = bufferevent_get_input(bev);
    prot_main_assign(pmain, bev);
}

int main()
{
    struct event_base *base;
    struct evconnlistener *listener;
    struct sockaddr_in sin;

    int port = DEEP_MESSENGER_PORT;

    debug_set_fp(stdout);

    base = event_base_new();

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(0); // Listen on 0.0.0.0

    listener = evconnlistener_new_bind(base, connect_cb, NULL,
                                       LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr *)&sin, sizeof(sin));

    if (!listener)
    {
        debug("Failed to bind listener");
    }

    event_base_dispatch(base);
    return 0;
}