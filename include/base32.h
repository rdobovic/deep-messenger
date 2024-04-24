#ifndef _INCLUDE_BASE32_H_
#define _INCLUDE_BASE32_H_

#include <stdint.h>
#include <stdlib.h>

#define BASE32_PAD_CHARACTER '='

#define BASE32_ENCODED_LEN(plain_len) \
    ((((plain_len) / 5) * 8) + ((plain_len) % 5 ? 8 : 0))

#define BASE32_DECODED_LEN(code_len) \
    ((code_len) / 8 * 5)

// Encode given binary data using base32 encoding
size_t base32_encode(const uint8_t *plain, size_t len, char *coded, int use_padding);

// Decode given base32 encoded string to 
size_t base32_decode(const char *coded, size_t len, uint8_t *plain);

// Check if given string is valid base32 encoded string
int base32_valid(const char *coded, int len);

#endif