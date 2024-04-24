#include <base32.h>
#include <stdint.h>
#include <debug.h>

int main() {
    int end;
    char out[100];
    char out_decoded[100];
    const char str[] = "i4mcwgorejxtforxrd7dsf73hsiiphhlgxxz3aeuef3hixdcv4vg3bid";

    debug_set_fp(stdout);

    /*end = base32_encode(str, 39, out, 0);
    out[end] = '\0';
    debug("%s", out);*/

    end = base32_decode(str, 56, out_decoded);
    out_decoded[end] = '\0';
    debug("len(%d) %s", end, out_decoded);

    return 0;
}