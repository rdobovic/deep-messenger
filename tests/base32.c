#include <base32.h>
#include <stdint.h>
#include <string.h>
#include <debug.h>

int main() {
    int end;
    char out[100];
    char out_decoded[100];
    const char str[] = "rdobovic_was_here";

    debug_set_fp(stdout);

    debug("STRING:  %s", str);

    end = base32_encode(str, strlen(str), out, 0);
    out[end] = '\0';
    debug("ENCODED: %s", out);

    end = base32_decode(out, end, out_decoded);
    out_decoded[end] = '\0';
    debug("DECODED: %s", out_decoded);

    return 0;
}