#include <array.h>
#include <debug.h>

#define ONION_ADDRESS "i4mcwgorejxtforxrd7dsf73hsiiphhlgxxz3aeuef3hixdcv4vg3bid.onion"

int main() {
    int *arr;
    debug_set_fp(stdout);

    void *ptr;

    arr = array(int);
    array_expand(arr, 20);
    array_length(arr);

    array_set(arr, 1330, 1300);

    arr[5] = 12;

    debug("Array len: %d", array_length(arr));

    array_free(arr);
    return 0;
}