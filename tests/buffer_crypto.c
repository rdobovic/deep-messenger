#include <buffer_crypto.h>
#include <helpers_crypto.h>
#include <stdint.h>
#include <string.h>
#include <debug.h>
#include <event2/buffer.h>
#include <openssl/evp.h>
#include <openssl/encoder.h>
#include <constants.h>
#include <openssl/err.h>
#include <base32.h>

void log_buffer(struct evbuffer *buff) {
    int i;
    size_t len;
    uint8_t *data;

    len = evbuffer_get_length(buff);
    data = evbuffer_pullup(buff, len);

    fprintf(_debug_file, "Buffer dump:\n");
    for (i = 0; i < len; i++) {
        fprintf(_debug_file, "%02x", data[i]);
    }
    fprintf(_debug_file, "\n");
}

int main() {
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx;
    struct evbuffer *buff;
    int valid = 0;
    int len;

    // -------------------------- ED25519

    size_t pub_len = ED25519_PUB_KEY_LEN;
    uint8_t pub[ED25519_PUB_KEY_LEN];
    size_t priv_len = ED25519_PRIV_KEY_LEN;
    uint8_t priv[ED25519_PRIV_KEY_LEN];

    char pub_enc[BASE32_ENCODED_LEN(ED25519_PUB_KEY_LEN) + 1];
    char priv_enc[BASE32_ENCODED_LEN(ED25519_PRIV_KEY_LEN) + 1];

    debug_set_fp(stdout);

    buff = evbuffer_new();
    evbuffer_add(buff, "Some Text", 10);

    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);

    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_generate(ctx, &pkey);

    EVP_PKEY_get_raw_public_key(pkey, pub, &pub_len);
    EVP_PKEY_get_raw_private_key(pkey, priv, &priv_len);

    len = base32_encode(pub, ED25519_PUB_KEY_LEN, pub_enc, 0);
    pub_enc[len] = '\0';
    len = base32_encode(priv, ED25519_PRIV_KEY_LEN, priv_enc, 0);
    priv_enc[len] = '\0';

    debug("Pub key: %s", pub_enc);
    debug("Priv key: %s", priv_enc);

    debug("Len before: %d", evbuffer_get_length(buff));

    ed25519_buffer_sign(buff, 0, priv);

    debug("Len after: %d", len = evbuffer_get_length(buff));

    evbuffer_add(buff, "Something", 10);
    //evbuffer_prepend(buff, "Bad data", 9);

    valid = ed25519_buffer_validate(buff, len, pub);
    debug("Valid: %d", valid);

    // ---------------------------------- RSA

    int i = 1;
    struct evbuffer *buff2, *buff3;
    uint8_t rpub_key[CLIENT_ENC_KEY_PUB_LEN];
    uint8_t rpriv_key[CLIENT_ENC_KEY_PRIV_LEN];

    buff2 = evbuffer_new();
    buff3 = evbuffer_new();
    evbuffer_drain(buff, evbuffer_get_length(buff));
    evbuffer_add(buff, "SomethingSomething", 19);

    rsa_2048bit_keygen(rpub_key, rpriv_key);

    debug("BUFF1 LEN: %d", evbuffer_get_length(buff));

    debug("Generated");
    rsa_buffer_encrypt(buff, rpub_key, buff2);

    debug("BUFF2 LEN: %d", evbuffer_get_length(buff2));

    debug("Encrypted");
    log_buffer(buff2);
    rsa_buffer_decrypt(buff2, rpriv_key, buff3);

    debug("BUFF3 LEN: %d", evbuffer_get_length(buff3));

    return 0;
}