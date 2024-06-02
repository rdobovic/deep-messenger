#include <onion.h>
#include <string.h>
#include <debug.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <db_init.h>
#include <db_options.h>

#define ONION_DIR "/home/roko/test/onion/onion_dir"

int main() {
    size_t len;
    uint8_t onion_address[ONION_ADDRESS_LEN + 1];
    uint8_t pub_key[ONION_PUB_KEY_LEN];
    uint8_t priv_key[ONION_PRIV_KEY_LEN];

    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *keyctx;

    debug_set_fp(stdout);
    db_init_global("deep_messenger2.db");
    db_init_schema(dbg);

    keyctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);

    EVP_PKEY_keygen_init(keyctx);
    EVP_PKEY_generate(keyctx, &pkey);

    len = ONION_PUB_KEY_LEN;
    EVP_PKEY_get_raw_public_key(pkey, pub_key, &len);

    len = ONION_PRIV_KEY_LEN;
    EVP_PKEY_get_raw_private_key(pkey, priv_key, &len);

    EVP_PKEY_CTX_free(keyctx);
    EVP_PKEY_free(pkey);

    if (!onion_init_dir(ONION_DIR, pub_key, priv_key))
        debug("Failed to init onion dir");

    onion_address_from_pub_key(pub_key, onion_address);
    onion_address[ONION_ADDRESS_LEN] = '\0';

    db_options_set_text(dbg, "onion_address", onion_address, strlen(onion_address));

    db_options_set_bin(dbg, "onion_public_key", pub_key, ONION_PUB_KEY_LEN);
    db_options_set_bin(dbg, "onion_private_key", priv_key, ONION_PRIV_KEY_LEN);

    return 0;
}