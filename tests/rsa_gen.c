#include <debug.h>
#include <helpers_crypto.h>
#include <stdint.h>
#include <constants.h>

int main() {
    int i = 0;
    debug_set_fp(stdout);
    uint8_t pub[CLIENT_ENC_KEY_PUB_LEN], priv[CLIENT_ENC_KEY_PRIV_LEN];

    while (1) {
        debug("%d", i++);
        rsa_2048bit_keygen(pub, priv);
    }
}