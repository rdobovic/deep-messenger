#include <stdio.h>
#include <stdlib.h>
#include <sys_memory.h>
#include <debug.h>

// Allocate memory and exit program on fail
void * safe_malloc(size_t chunk_size, const char *error_str) {
    void *ptr;

    if (!(ptr = malloc(chunk_size))) {
        sys_memory_crash(error_str);
    }
    return ptr;
}

// Reallocate memory and exit program on fail
void * safe_realloc(void *old_ptr, size_t chunk_size, const char *error_str) {
    void *ptr;

    if (!(ptr = realloc(old_ptr, chunk_size))) {
        sys_memory_crash(error_str);
    }
    return ptr;
}