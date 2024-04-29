#ifndef _INCLUDE_PROT_ACK_H_
#define _INCLUDE_PROT_ACK_H_

#include <stdint.h>
#include <prot_main.h>
#include <constants.h>

typedef void (*prot_ack_ed25519_cb)(int ack_success, void *cbarg);

struct prot_ack_ed25519 {
    int ack_success;
    uint8_t pub_key[ED25519_PUB_KEY_LEN];
    uint8_t priv_key[ED25519_PRIV_KEY_LEN];

    void *cbarg;
    prot_ack_ed25519_cb cb;
    enum prot_message_codes msg_code;

    struct prot_tran_handler htran;
    struct prot_recv_handler hrecv;
};

// Allocate new ack handler
struct prot_ack_ed25519 * prot_ack_ed25519_new(
    enum prot_message_codes msg_code,
    uint8_t *pub_key,
    uint8_t *priv_key,
    prot_ack_ed25519_cb cb,
    void *cbarg
);

// Free memory for given ack
void prot_ack_ed25519_free(struct prot_ack_ed25519 *msg);

#endif