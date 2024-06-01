#include <array.h>
#include <stdlib.h>
#include <debug.h>
#include <sys_memory.h>

// Create new dynamic array
void * _array_new(size_t el_size) {
    void *arr_void;
    struct array_data *arr;

    arr_void = safe_malloc(sizeof(struct array_data) + el_size * ARRAY_CHUNK_SIZE,
        "Failed to allocate dynamic array");

    arr = arr_void;
    arr->element_size = el_size;
    arr->length = ARRAY_CHUNK_SIZE;

    arr_void += sizeof(struct array_data);
    return arr_void;
}

// Expand array to have at least given size
void * _array_expand(void *arr_s, int new_length) {
    // Get original (malloc) array pointer
    void *arr_void = arr_s - sizeof(struct array_data);
    // Get structure pointer
    struct array_data *arr = arr_void;

    if (arr->length < new_length) {
        arr->length = (new_length / ARRAY_CHUNK_SIZE + 1) * ARRAY_CHUNK_SIZE;

        arr_void = safe_realloc(arr_void, sizeof(struct array_data) + arr->element_size * arr->length,
            "Failed to expand dynamic array");
    }

    arr_void += sizeof(struct array_data);
    return arr_void;
}

// Free given dynamic array
void array_free(void *arr_s) {
    if (arr_s)
        free(arr_s - sizeof(struct array_data));
}

// Get current length of given array (elements allocated)
int array_length(void *arr_s) {
    struct array_data *arr = arr_s - sizeof(struct array_data);
    return arr->length;
}

// Copy given null terminated string into array, expand array as needed
// function copies until it reaches null terminator or maxlen if maxlen is not -1
void * _array_strcpy(void *arr_s, const char *str, int maxlen) {
    int len, i;
    char *dest;

    if (maxlen == -1)
        maxlen = strlen(str);

    array_expand(arr_s, maxlen + 1);
    dest = arr_s;
    
    for (i = 0; i < maxlen; i++) {
        if (str[i] == '\0')
            break;

        dest[i] = str[i];
    }
    dest[i] = '\0';
    return arr_s;
}