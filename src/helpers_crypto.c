#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/encoder.h>
#include <openssl/decoder.h>
#include <constants.h>
#include <helpers_crypto.h>
#include <debug.h>

// Generate ED25519 keypair and place keys on given locations
void ed25519_keygen(uint8_t *public_key, uint8_t *private_key) {
    size_t len;
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *keyctx;

    if (
        !(keyctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL)) ||
        !EVP_PKEY_keygen_init(keyctx) ||
        !EVP_PKEY_generate(keyctx, &pkey)
    )
        sys_openssl_crash("Failed to generate ED25519 keypair");

    len = CLIENT_SIG_KEY_PUB_LEN;
    if (!EVP_PKEY_get_raw_public_key(pkey, public_key, &len))
        sys_openssl_crash("Failed to extract public ED25519 key");

    len = CLIENT_SIG_KEY_PRIV_LEN;
    if (!EVP_PKEY_get_raw_private_key(pkey, private_key, &len))
        sys_openssl_crash("Failed to extract private ED25519 key");

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(keyctx);
}

// Generate 2048bit RSA keypair, encode them in DER format and store keys
// to given locations, key lengths are defined by macros
void rsa_2048bit_keygen(uint8_t *public_key, uint8_t *private_key) {
    size_t len;
    uint8_t *key_ptr;
    EVP_PKEY *pkey;
    OSSL_ENCODER_CTX *encctx;

    // Generate RSA 2048bit keypair
    if (!(pkey = EVP_RSA_gen(2048)))
        sys_openssl_crash("Failed to create RSA keypair");

    if (!(encctx = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_KEYPAIR, "DER", NULL, NULL)))
        sys_openssl_crash("Failed to create DER encoder for private key");

    len = CLIENT_ENC_KEY_PRIV_LEN;
    key_ptr = private_key;
    if (!OSSL_ENCODER_to_data(encctx, &key_ptr, &len)) {
        debug("key_ptr(%p), len(%d)", key_ptr, len);
        sys_openssl_crash("Failed to encode RSA private key to DER");
    }
    OSSL_ENCODER_CTX_free(encctx);

    if (len != CLIENT_ENC_KEY_PRIV_LEN - 1192)
        debug("Wrong size %d", CLIENT_ENC_KEY_PRIV_LEN - len);

    if (!(encctx = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, "DER", NULL, NULL)))
        sys_openssl_crash("Failed to create DER encoder for public key");

    len = CLIENT_ENC_KEY_PUB_LEN;
    key_ptr = public_key;
    if (!OSSL_ENCODER_to_data(encctx, &key_ptr, &len))
        sys_openssl_crash("Failed to encode RSA public key to DER");
    OSSL_ENCODER_CTX_free(encctx);

    EVP_PKEY_free(pkey);
}

// Decodes given RSA public key encoded in DER format and returns pointer to
// EVP_PKEY on success or NULL on failure
EVP_PKEY * rsa_2048bit_pub_key_decode(uint8_t *public_key) {
    int is_err = 0;
    size_t len;
    const uint8_t *der_ptr;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *decctx = NULL;

    if (!(decctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, "DER", NULL, "RSA", EVP_PKEY_PUBLIC_KEY, NULL, NULL))) {
        is_err = 1; goto err;
    }

    len = CLIENT_ENC_KEY_PUB_LEN;
    der_ptr = public_key;
    if(!OSSL_DECODER_from_data(decctx, &der_ptr, &len)) {
        is_err = 1; goto err;
    }

    err:
    OSSL_DECODER_CTX_free(decctx);

    if (is_err)
        return NULL;
    return pkey;
}

// Decodes given RSA private key encoded in DER format and returns pointer to
// EVP_PKEY on success or NULL on failure
EVP_PKEY * rsa_2048bit_priv_key_decode(uint8_t *private_key) {
    int is_err = 0;
    size_t len;
    const uint8_t *der_ptr;
    EVP_PKEY *pkey = NULL;
    OSSL_DECODER_CTX *decctx = NULL;

    if (!(decctx = OSSL_DECODER_CTX_new_for_pkey(&pkey, "DER", NULL, "RSA", EVP_PKEY_KEYPAIR, NULL, NULL))) {
        is_err = 1; goto err;
    }

    len = CLIENT_ENC_KEY_PRIV_LEN;
    der_ptr = private_key;
    if(!OSSL_DECODER_from_data(decctx, &der_ptr, &len)) {
        is_err = 1; goto err;
    }

    err:
    OSSL_DECODER_CTX_free(decctx);

    if (is_err)
        return NULL;
    return pkey;
}