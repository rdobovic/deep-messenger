#include <stdint.h>
#include <sys_memory.h>
#include <constants.h>
#include <prot_ack.h>
#include <prot_main.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <buffer_crypto.h>
#include <debug.h>

// Free ACK handler memory
static void tran_cleanup(struct prot_tran_handler *phand) {
    struct prot_ack_ed25519 *ack = phand->msg;

    if (!ack->ack_success && ack->cb)
        ack->cb(0, ack->cbarg);

    prot_ack_ed25519_free(ack);
}

// Called if ACK transmission is succesfully completed
static void tran_done(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_ack_ed25519 *ack = phand->msg;

    ack->ack_success = 1;
    if (ack->cb)
        ack->cb(1, ack->cbarg);
}

// Fill ACK buffer
static void tran_setup(struct prot_main *pmain, struct prot_tran_handler *phand) {
    struct prot_ack_ed25519 *ack = phand->msg;

    debug("Preparing ack transmission");

    evbuffer_add(phand->buffer, prot_header(ack->msg_code), PROT_HEADER_LEN);
    evbuffer_add(phand->buffer, pmain->transaction_id, TRANSACTION_ID_LEN);
    ed25519_buffer_sign(phand->buffer, 0, ack->priv_key);
}

// Free the ACK handler memory
static void recv_cleanup(struct prot_recv_handler *phand) {
    struct prot_ack_ed25519 *ack = phand->msg;

    if (!ack->ack_success && ack->cb)
        ack->cb(0, ack->cbarg);

    prot_ack_ed25519_free(ack);
}

// Handle incomming data
static void recv_handle(struct prot_main *pmain, struct prot_recv_handler *phand) {
    struct evbuffer *input;
    struct prot_ack_ed25519 *ack = phand->msg;
    int message_len = PROT_HEADER_LEN + TRANSACTION_ID_LEN + ED25519_SIGNATURE_LEN;

    debug("Hello");

    input = bufferevent_get_input(pmain->bev);

    if (evbuffer_get_length(input) < message_len)
        return;

    if (!ed25519_buffer_validate(input, message_len, ack->pub_key)) {
        prot_main_set_error(pmain, PROT_ERR_INVALID_MSG);
        return;
    }

    ack->ack_success = 1;
    if (ack->cb)
        ack->cb(1, ack->cbarg);

    evbuffer_drain(input, message_len);
    pmain->current_recv_done = 1;
}

// Allocate new ack handler
struct prot_ack_ed25519 * prot_ack_ed25519_new(
    enum prot_message_codes msg_code,
    uint8_t *pub_key,
    uint8_t *priv_key,
    prot_ack_ed25519_cb cb,
    void *cbarg
) {
    struct prot_ack_ed25519 *ack;

    ack = safe_malloc(sizeof(struct prot_ack_ed25519), "Failed to allocate ACK message");
    memset(ack, 0, sizeof(struct prot_ack_ed25519));
    
    ack->cb = cb;
    ack->cbarg = cbarg;
    ack->msg_code = msg_code;
    if (pub_key)
        memcpy(ack->pub_key, pub_key, ED25519_PUB_KEY_LEN);
    if (priv_key)
        memcpy(ack->priv_key, priv_key, ED25519_PRIV_KEY_LEN);

    ack->htran.msg = ack;
    ack->htran.msg_code = msg_code;
    ack->htran.done_cb = tran_done;
    ack->htran.setup_cb = tran_setup;
    ack->htran.cleanup_cb = tran_cleanup;
    ack->htran.buffer = evbuffer_new();

    ack->hrecv.msg = ack;
    ack->hrecv.msg_code = msg_code;
    ack->hrecv.require_transaction = 1;
    ack->hrecv.handle_cb = recv_handle;
    ack->hrecv.cleanup_cb = recv_cleanup;

    return ack;
}

// Free memory for given ack
void prot_ack_ed25519_free(struct prot_ack_ed25519 *ack) {
    if (ack && ack->htran.buffer)
        evbuffer_free(ack->htran.buffer);
    free(ack);
}