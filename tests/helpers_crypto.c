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

#include <db_contact.h>
#include <db_init.h>

#define MAX_BUFFER 270


int main(void) {
    debug_init();
    debug_set_fp(stdout);

    int rc;
    struct evbuffer *buff_in, *buff_out, *buff_plain;
    
    uint8_t e_pub[CLIENT_ENC_KEY_PUB_LEN];
    uint8_t e_priv[CLIENT_ENC_KEY_PRIV_LEN];

    char *out;
    const char in[] = "This is my str :)";

    struct db_contact *c1, *c2;

    db_init_global("random.db");
    db_init_schema(dbg);

    c1 = db_contact_new();
    rsa_2048bit_keygen(c1->local_enc_key_pub, c1->local_enc_key_priv);
    db_contact_save(dbg, c1);

    c2 = db_contact_get_by_pk(dbg, c1->id, NULL);

    debug("K1: %02x %02x, K2: %02x %02x", c1->local_enc_key_priv[0], c1->local_enc_key_priv[1], c2->local_enc_key_priv[0], c2->local_enc_key_priv[1]);

    buff_in = evbuffer_new();
    buff_out = evbuffer_new();
    buff_plain = evbuffer_new();

    debug("In: %s", in);
    evbuffer_add(buff_in, in, sizeof(in));

    rc = rsa_buffer_encrypt(buff_in, c2->local_enc_key_pub, buff_out, NULL);
    debug("Encryption err: %d", rc);

    //evbuffer_add(buff_out, "Random text", 12);

    rc = rsa_buffer_decrypt(buff_out, c2->local_enc_key_priv, buff_plain, NULL);
    debug("Decryption err: %d", rc);

    out = evbuffer_pullup(buff_plain, evbuffer_get_length(buff_plain));
    debug("Out: %s", out);
}
