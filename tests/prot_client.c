#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include <debug.h>
#include <constants.h>
#include <prot_main.h>
#include <prot_transaction.h>
#include <prot_ack.h>
#include <base32.h>

#define PRIV_KEY "7d2ckcarsvublbqu2dhcenljx3i2uy4wk5uhc2yuu66f76wkadnq"

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

    struct prot_ack_ed25519 *ack;
    uint8_t priv_key[ED25519_PRIV_KEY_LEN];

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

    pmain = prot_main_new(base);
    prot_main_assign(pmain, bev);
    prot_main_setcb(pmain, pmain_done_cb, NULL, NULL);

    treq = prot_txn_req_new();
    prot_main_push_tran(pmain, &(treq->htran));

    base32_decode(PRIV_KEY, strlen(PRIV_KEY), priv_key);
    ack = prot_ack_ed25519_new(PROT_ACK_SIGNATURE, NULL, priv_key, ack_cb, NULL);
    prot_main_push_tran(pmain, &(ack->htran));

    event_base_dispatch(base);
    return 0;
}