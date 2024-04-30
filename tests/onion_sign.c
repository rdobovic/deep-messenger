#include <onion.h>
#include <debug.h>
#include <stdio.h>
#include <sys_crash.h>
#include <constants.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <sqlite3.h>

// This one is useless

int main() {
    int v = 0;
    FILE *fp;
    char msg[] = "Test";
    size_t sig_len = 2000;
    uint8_t signature[2000];

    uint8_t pub_key[ONION_PUB_KEY_LEN + 1];
    uint8_t priv_key[ONION_PUB_KEY_LEN + 1];

    debug("Your onion private ED25519 key is: %s", priv_key);

    EVP_PKEY *pkey_pub, *pkey_priv;

    pkey_pub = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub_key, ONION_PUB_KEY_LEN);
    debug("Public key p(%p)", pkey_pub);
    pkey_priv = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, priv_key, ONION_PRIV_KEY_LEN);
    debug("Private key p(%p)", pkey_priv);

    EVP_MD_CTX *ctx;

    ctx = EVP_MD_CTX_new();

    EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey_priv);
    EVP_DigestSign(ctx, signature, &sig_len, msg, 4);

    EVP_MD_CTX_free(ctx);

    ctx = EVP_MD_CTX_new();

    EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey_pub);
    v = EVP_DigestVerify(ctx, signature, sig_len, msg, 4);

    debug("ERR: %s", ERR_error_string(ERR_get_error(), NULL));

    debug("Valid: %d", v);

    return 0;
}