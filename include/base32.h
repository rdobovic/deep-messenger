#ifndef _INCLUDE_BASE32_H_
#define _INCLUDE_BASE32_H_

#include <stdint.h>
#include <stdlib.h>

#define BASE32_PAD_CHARACTER '='

#define BASE32_ENCODED_LEN(plain_len) ((plain_len) )

// Encode given binary data using base32 encoding
size_t base32_encode(const uint8_t *plain, size_t len, char *coded, int use_padding);

// Decode given base32 encoded string to 
size_t base32_decode(const char *coded, size_t len, uint8_t *plain);

#endif