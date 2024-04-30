#include <stdio.h>
#include <stdlib.h>
#include <debug.h>
#include <openssl/evp.h>
#include <openssl/encoder.h>
#include <openssl/decoder.h>
#include <openssl/err.h>
#include <openssl/rsa.h>

#define MAX_BUFFER 270


int main(void) {
    int temp;
    unsigned char pub_key_der[MAX_BUFFER];
    unsigned char *pkd_ptr = pub_key_der;
    size_t pkd_len = MAX_BUFFER;
    size_t priv_len = 0, pub_len = 0;
    
    debug_init();
    debug_set_fp(stdout);

    debug("Openssl start");

    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = NULL;

    pkey = EVP_RSA_gen(2048);

    // Create new encoder context for encoding RSA pub key to DER
    OSSL_ENCODER_CTX *encctx;

    if (!(encctx = OSSL_ENCODER_CTX_new_for_pkey(pkey, EVP_PKEY_PUBLIC_KEY, "DER", NULL, NULL)))
        debug("Failed to create encoder context");

    // Encode data to char array
    if (!OSSL_ENCODER_to_data(encctx, &pkd_ptr, &pkd_len))
        debug("Encoding failed");

    debug("Encoded key length: %ld", MAX_BUFFER - pkd_len);

    OSSL_ENCODER_CTX_free(encctx);

    // Create new decoder context for decodig RSA pub key from DER
    EVP_PKEY *public_key = NULL;
    OSSL_DECODER_CTX *decctx;
    
    pkd_len = MAX_BUFFER - pkd_len;
    const unsigned char *pkd_ptr2 = pub_key_der;

    if (!(decctx = OSSL_DECODER_CTX_new_for_pkey(&public_key, "DER", NULL, "RSA", EVP_PKEY_PUBLIC_KEY, NULL, NULL)))
        debug("Failed to create decoder context");

    // Decode key from char array
    if (!OSSL_DECODER_from_data(decctx, &pkd_ptr2, &pkd_len))
        debug("Public key decode failed");

    OSSL_DECODER_CTX_free(decctx);

    // Lets encrypt something
    EVP_CIPHER_CTX *cipctx = NULL;
    unsigned char ek[1000];
    unsigned char *ekp = ek;
    unsigned char **ekpp = &ekp;

    unsigned char iv[1000];
    int ekl = 1000;
    char encrypted[MAX_BUFFER];
    int encrypted_len = MAX_BUFFER;

    debug("BEFORE HERE");

    cipctx = EVP_CIPHER_CTX_new();
    if (!EVP_SealInit(cipctx, EVP_aes_256_cbc(), ekpp, &ekl, iv, &public_key, 1))
        debug("Failed to init Seal");

    debug("AES iv len: %d", EVP_CIPHER_get_iv_length(EVP_aes_256_cbc()));
    debug("AES ecrypted key len: %d >= %d", ekl, EVP_PKEY_get_size(public_key));


    if (!EVP_SealUpdate(cipctx, encrypted, &encrypted_len, "This is my test string :)", 26))
        debug("Failed to encrypt data");

    temp = encrypted_len;
    if (!EVP_SealFinal(cipctx, encrypted + encrypted_len, &encrypted_len))
        debug("Seal final failed");

    encrypted_len += temp;
    debug("Encrypted len: %d", encrypted_len);
    EVP_CIPHER_CTX_free(cipctx);

    EVP_CIPHER_CTX *opnctx = NULL;
    unsigned char plaintext[MAX_BUFFER];
    int plaintext_len = MAX_BUFFER;
    int enc_len = encrypted_len;

    opnctx = EVP_CIPHER_CTX_new();
    if (!EVP_OpenInit(opnctx, EVP_aes_256_cbc(), ekpp[0], ekl, iv, pkey))
        debug("Failed to init Open: %s", ERR_error_string(ERR_get_error(), NULL));

    //ERR_lib_error_string()

    if (EVP_OpenUpdate(opnctx, plaintext, &plaintext_len, encrypted, encrypted_len) == 0)
        debug("Decryption step failed %s", ERR_error_string(ERR_get_error(), NULL));

    temp = plaintext_len;
    if (!EVP_OpenFinal(opnctx, plaintext + plaintext_len, &plaintext_len))
        debug("Open final failed");

    plaintext_len += temp;
    debug("Plaintext len: %d", plaintext_len);
    plaintext[plaintext_len] = '\0';

    debug("Plaintext: %s", plaintext);
    EVP_CIPHER_CTX_free(opnctx);

    EVP_PKEY_free(pkey);
    EVP_PKEY_free(public_key);
    

    


    /*
    if (ctx == NULL) {
        debug("Context failed");
    } else {
        debug("Context success priv(%ld) pub(%ld)", priv_len, pub_len);
    }
    */
}
