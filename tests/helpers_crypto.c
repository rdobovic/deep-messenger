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

    int rc;
    struct evbuffer *buff_in, *buff_out, *buff_plain;
    
    uint8_t e_pub[CLIENT_ENC_KEY_PUB_LEN];
    uint8_t e_priv[CLIENT_ENC_KEY_PRIV_LEN];

    char *out;
    const char in[] = "This is my str :), how are you doing? So this is magic.";

    rsa_2048bit_keygen(e_pub, e_priv);

    buff_in = evbuffer_new();
    buff_out = evbuffer_new();
    buff_plain = evbuffer_new();

    debug("In: %s", in);
    evbuffer_add(buff_in, in, sizeof(in));

    rc = rsa_buffer_encrypt(buff_in, e_pub, buff_out, NULL);
    debug("Encryption err: %d", rc);

    evbuffer_add(buff_out, "Random text", 12);

    rc = rsa_buffer_decrypt(buff_out, e_priv, buff_plain, NULL);
    debug("Decryption err: %d", rc);

    out = evbuffer_pullup(buff_plain, evbuffer_get_length(buff_plain));
    debug("Out: %s", out);
}
