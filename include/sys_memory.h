#ifndef _INCLUDE_SYS_MEMORY_H_
#define _INCLUDE_SYS_MEMORY_H_

#include <string.h>
#include <stdlib.h>
#include <sys_crash.h>

#define CRASH_SOURCE_MEMORY "Memory error"

// Zero out given variable of given type
#define zero_chunk(ptr, chunk_type) \
    memset((ptr), 0, sizeof(chunk_type))

// Zero out specified number of bytes
#define zero_bytes(ptr, n_bytes) \
    memset((ptr), 0, (n_bytes))

// Crash application with given memory error
#define sys_memory_crash(...) \
    sys_crash(CRASH_SOURCE_MEMORY, __VA_ARGS__)

// Allocate memory and exit program on fail
void * safe_malloc(size_t chunk_size, const char *error_str);

// Reallocate memory and exit program on fail
void * safe_realloc(void * ptr, size_t chunk_size, const char *error_str);

#endif
