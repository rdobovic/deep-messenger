#include <stdio.h>
#include <stdlib.h>
#include <debug.h>
#include <openssl/evp.h>
#include <openssl/encoder.h>
#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/rsa.h>

#include <constants.h>
#include <helpers_crypto.h>
#include <buffer_crypto.h>
#include <sys_memory.h>

#define MAX_BUFFER 270


int main(void) {
    debug_init();
    debug_set_fp(stdout);

    debug("Openssl start");

    int temp;
    uint8_t encrypted[2000];
    struct evbuffer *buff_in, *buff_out;

    EVP_PKEY *pkey_pub = NULL;
    EVP_PKEY *pkey_priv = NULL;
    
    uint8_t e_pub[CLIENT_ENC_KEY_PUB_LEN];
    uint8_t e_priv[CLIENT_ENC_KEY_PRIV_LEN];

    EVP_CIPHER_CTX *cipctx = NULL;

    rsa_2048bit_keygen(e_pub, e_priv);

    pkey_pub = rsa_2048bit_pub_key_decode(e_pub);
    pkey_priv = rsa_2048bit_priv_key_decode(e_priv);

    int ekl, ivl;

    // Lets encrypt something
    uint8_t *ek;
    uint8_t *iv;

    ekl = EVP_PKEY_get_size(pkey_priv);
    ivl = EVP_CIPHER_get_iv_length(EVP_aes_256_cbc());

    ek = safe_malloc(ekl, "FAIL");
    iv = safe_malloc(ivl, "FAIL");

    buff_in = evbuffer_new();
    buff_out = evbuffer_new();

    uint32_t encrypted_len;
    struct evbuffer_ptr pos;
    evbuffer_add(buff_in, "This is my str :), how are you doing?", 38);

    rsa_buffer_encrypt(buff_in, e_pub, buff_out);
    debug("Encrypted");
    /*
    evbuffer_copyout(buff_out, &encrypted_len, sizeof(encrypted_len));
    encrypted_len = ntohl(encrypted_len);

    evbuffer_ptr_set(buff_out, &pos, sizeof(encrypted_len) + encrypted_len, EVBUFFER_PTR_SET);
    evbuffer_copyout_from(buff_out, &pos, ek, ekl);
    evbuffer_ptr_set(buff_out, &pos, sizeof(encrypted_len) + encrypted_len + ekl, EVBUFFER_PTR_SET);
    evbuffer_copyout_from(buff_out, &pos, iv, ivl);

    size_t len;
    int n_vec_enc;
    struct evbuffer_iovec *vec_enc;
    struct evbuffer_iovec vec_plain;

    evbuffer_ptr_set(buff_out, &pos, sizeof(encrypted_len), EVBUFFER_PTR_SET);
    n_vec_enc = evbuffer_peek(buff_out, encrypted_len, &pos, NULL, 0);
    vec_enc = safe_malloc(sizeof(struct evbuffer_iovec) * n_vec_enc, "FAIL");
    n_vec_enc = evbuffer_peek(buff_out, encrypted_len, &pos, vec_enc, n_vec_enc);

    debug("BUFF LEN: %d", evbuffer_get_length(buff_out));
    */
    EVP_CIPHER_CTX *opnctx = NULL;
    unsigned char plaintext[MAX_BUFFER];
    int plaintext_len = MAX_BUFFER;
    int enc_len = encrypted_len;
    int i;

    struct evbuffer *plain;
    plain = evbuffer_new();

    /*opnctx = EVP_CIPHER_CTX_new();
    if (!EVP_OpenInit(opnctx, EVP_aes_256_cbc(), ek, ekl, iv, pkey_priv))
        debug("Failed to init Open: %s", ERR_error_string(ERR_get_error(), NULL));

    //ERR_lib_error_string()

    for (i = 0; i < n_vec_enc; i++) {
        len = (vec_enc[i].iov_len < encrypted_len) ? vec_enc[i].iov_len : encrypted_len;
        debug("Chunk size: %d", len);

        evbuffer_reserve_space(plain, len, &vec_plain, 1);

        //vec_plain = evbuffer_reserve_space(plain, vec_enc[i].iov_len, 1);
        temp = vec_plain.iov_len;
        if (EVP_OpenUpdate(opnctx, vec_plain.iov_base, &temp, vec_enc[i].iov_base, len) == 0)
            debug("Decryption step failed %s", ERR_error_string(ERR_get_error(), NULL));
        vec_plain.iov_len = temp;
        evbuffer_commit_space(plain, &vec_plain, 1);
    }

    //if (EVP_OpenUpdate(opnctx, plaintext, &plaintext_len, encrypted, encrypted_len) == 0)
        //debug("Decryption step failed %s", ERR_error_string(ERR_get_error(), NULL));

    evbuffer_reserve_space(plain, EVP_CIPHER_block_size(EVP_aes_256_cbc()), &vec_plain, 1);
    temp = vec_plain.iov_len;
    if (!EVP_OpenFinal(opnctx, vec_plain.iov_base, &temp))
        debug("Open final failed");
    vec_plain.iov_len = temp;
    evbuffer_commit_space(plain, &vec_plain, 1);*/

    debug("Decode: %d", rsa_buffer_decrypt(buff_out, e_priv, plain));

    plaintext_len = evbuffer_get_length(plain);
    evbuffer_remove(plain, plaintext, plaintext_len);

    debug("Plaintext len: %d", plaintext_len);
    debug("Plaintext: %s", plaintext);
    EVP_CIPHER_CTX_free(opnctx);

    EVP_PKEY_free(pkey_pub);
    EVP_PKEY_free(pkey_priv);
    

    


    /*
    if (ctx == NULL) {
        debug("Context failed");
    } else {
        debug("Context success priv(%ld) pub(%ld)", priv_len, pub_len);
    }
    */
}
