#ifndef _INCLUDE_ARRAY_H_
#define _INCLUDE_ARRAY_H_

#include <stdlib.h>
#include <stdint.h>

#define ARRAY_CHUNK_SIZE 8

// Allocate new array for given type
#define array(type) \
    _array_new(sizeof(type))

// Access given element of array, expand if needed
#define array_at(arr, i) \
    (arr)[(i) + ((uintptr_t)((arr) = _array_expand((arr), (i) + 1)) * 0)]

// Set given element of array, expand if needed
#define array_set(arr, i, value) \
    (array_at((arr), (i)) = (value))

// Expand given array to have at least new length
#define array_expand(arr, new_length) \
    ((arr) = _array_expand((arr), (new_length)))

struct array_data {
    int length;
    size_t element_size;
};

// Create new dynamic array
void * _array_new(size_t el_size);

// Expand array to have at least given size
void * _array_expand(void *arr_s, int new_length);

// Free given dynamic array
void array_free(void *arr_s);

// Get current length of given array (elements allocated)
int array_length(void *arr_s);

// Macro for function below
#define array_strcpy(arr, str, maxlen) \
    ((arr) = _array_strcpy((arr), (str), (maxlen)))

// Copy given null terminated string into array, expand array as needed
// function copies until it reaches null terminator or maxlen if maxlen is not -1
void * _array_strcpy(void *arr_s, const char *str, int maxlen);

#endif