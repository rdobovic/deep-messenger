#include <stdlib.h>
#include <stdint.h>
#include <event2/buffer.h>
#include <buffer_crypto.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <sys_memory.h>
#include <constants.h>
#include <debug.h>
#include <helpers.h>

// Take first len bytes in the given buffer, sign them using provided ed25519
// private key and add signature to the end of buffer
int ed25519_buffer_sign(struct evbuffer *buff, size_t len, uint8_t *priv_key) {
    int i, is_err = 0;
    int n_iv;
    struct evbuffer_iovec *iv = NULL;

    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *ctx = NULL;
    EVP_MD_CTX *hashctx = NULL;

    uint8_t hash[EVP_MAX_MD_SIZE];
    int hash_len = EVP_MAX_MD_SIZE;

    uint8_t sig[ED25519_SIGNATURE_LEN];
    size_t sig_len = ED25519_SIGNATURE_LEN;

    if (len == 0)
        len = evbuffer_get_length(buff);

    hashctx = EVP_MD_CTX_new();
    if (!EVP_DigestInit_ex2(hashctx, EVP_sha512(), NULL)) {
        is_err = 1; goto err;
    }

    if (!(pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, priv_key, ED25519_PRIV_KEY_LEN))) {
        is_err = 1; goto err;
    }

    ctx = EVP_MD_CTX_new();
    if (!EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey)) {
        is_err = 1; goto err;
    }

    n_iv = evbuffer_peek(buff, len, NULL, NULL, 0);
    iv = safe_malloc((sizeof(struct evbuffer_iovec) * n_iv),
        "Failed to allocate memory for evbuffer iovec(s), on buffer sign");

    n_iv = evbuffer_peek(buff, len, NULL, iv, n_iv);

    for (i = 0; i < n_iv; i++) {
        if (!EVP_DigestUpdate(hashctx, iv[i].iov_base, min(iv[i].iov_len, len))) {
            is_err = 1; goto err;
        }
        len -= iv[i].iov_len;
    }

    if (!EVP_DigestFinal_ex(hashctx, hash, &hash_len)) {
        is_err = 1; goto err;
    }

    EVP_DigestSign(ctx, sig, &sig_len, hash, hash_len);
    evbuffer_add(buff, sig, sig_len);

    err:
    free(iv);
    EVP_MD_CTX_free(ctx);
    EVP_MD_CTX_free(hashctx);
    EVP_PKEY_free(pkey);

    if (is_err) {
        debug("An error occured while signing the buffer: %s", 
            ERR_error_string(ERR_get_error(), NULL));
        return 1;
    }

    return 0;
}

// Takes last ED25519_SIGNATURE_LEN bytes as a ed25519 signature and validates
// it against other data using provided ed25519 public key
int ed25519_buffer_validate(struct evbuffer *buff, size_t len, uint8_t *pub_key) {
    int i, j, n_iv, i_sig;
    int is_err = 0, is_valid = 0;
    struct evbuffer_ptr ptr;
    struct evbuffer_iovec *iv;
    size_t content_len = len - ED25519_SIGNATURE_LEN;

    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *ctx = NULL;
    EVP_MD_CTX *hashctx = NULL;

    uint8_t hash[EVP_MAX_MD_SIZE];
    int hash_len = EVP_MAX_MD_SIZE;

    uint8_t sig[ED25519_SIGNATURE_LEN];
    size_t sig_len = ED25519_SIGNATURE_LEN;

    if (len == 0)
        len = evbuffer_get_length(buff);

    if (!(pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub_key, ED25519_PUB_KEY_LEN))) {
        is_err = 1; goto err;
    }

    hashctx = EVP_MD_CTX_new();
    if (!EVP_DigestInit_ex2(hashctx, EVP_sha512(), NULL)) {
        is_err = 1; goto err;
    }

    evbuffer_ptr_set(buff, &ptr, content_len, EVBUFFER_PTR_SET);

    n_iv = evbuffer_peek(buff, ED25519_SIGNATURE_LEN, &ptr, NULL, 0);
    iv = safe_malloc((sizeof(struct evbuffer_iovec) * n_iv),
        "Failed to allocate memory for evbuffer iovec(s), on buffer verify");
    n_iv = evbuffer_peek(buff, ED25519_SIGNATURE_LEN, &ptr, iv, n_iv);

    i_sig = 0;
    for (i = 0; i < n_iv; i++) {
        for (j = 0; j < iv[i].iov_len; j++) {
            sig[i_sig++] = ((uint8_t *)iv[i].iov_base)[j];
        }
    }

    free(iv);

    n_iv = evbuffer_peek(buff, content_len, NULL, NULL, 0);
    iv = safe_malloc((sizeof(struct evbuffer_iovec) * n_iv),
        "Failed to allocate memory for evbuffer iovec(s), on buffer verify");
    n_iv = evbuffer_peek(buff, content_len, NULL, iv, n_iv);

    for (i = 0; i < n_iv; i++) {
        if (!EVP_DigestUpdate(hashctx, iv[i].iov_base, min(iv[i].iov_len, content_len))) {
            is_err = 1; goto err;
        }
        content_len -= iv[i].iov_len;
    }

    if (!EVP_DigestFinal_ex(hashctx, hash, &hash_len)) {
        is_err = 1; goto err;
    }

    ctx = EVP_MD_CTX_new();
    if (!EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey)) {
        is_err = 1; goto err;
    };

    is_valid = EVP_DigestVerify(ctx, sig, ED25519_SIGNATURE_LEN, hash, hash_len);

    err:
    free(iv);
    EVP_MD_CTX_free(ctx);
    EVP_MD_CTX_free(hashctx);
    EVP_PKEY_free(pkey);

    if (is_err) {
        debug("An error occured while checking buffer signature: %s", 
            ERR_error_string(ERR_get_error(), NULL));
        return 0;
    }

    return is_valid;
}