#include <onion.h>
#include <debug.h>

#define ONION_ADDRESS "i4mcwgorejxtforxrd7dsf73hsiiphhlgxxz3aeuef3hixdcv4vg3bid.onion"

int main() {
    int v = 0;
    debug_set_fp(stdout);

    v = onion_address_valid(ONION_ADDRESS);
    debug("VALID: %d", v);

    return 0;
}