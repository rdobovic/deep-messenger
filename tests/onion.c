#include <onion.h>
#include <debug.h>

#define ONION_ADDRESS "i4mcwgorejxtforxrd7dsf73hsiiphhlgxxz3aeuef3hixdcv4vg3bid.onion"

int main() {
    int v = 0;
    uint8_t onion_key[ONION_KEY_LEN + 1];
    debug_set_fp(stdout);

    v = onion_address_valid(ONION_ADDRESS);
    debug("Valid: %d", v);

    if (!v) return 0;

    onion_extract_key(ONION_ADDRESS, onion_key);
    debug("Your onion ED25519 key is: %s", onion_key);
    return 0;
}