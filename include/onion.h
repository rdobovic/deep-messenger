#ifndef _INCLUDE_ONION_H_
#define _INCLUDE_ONION_H_

#include <stdint.h>

#define ONION_VERSION      3
#define ONION_CHECKSUM_LEN 2
#define ONION_KEY_LEN      32
#define ONION_BASE_LEN     56
#define ONION_ADDRESS_LEN  62 // 56 (address) + 1 (dot) + 5 (onion text)

// Checks if onion domain is valid
int onion_address_valid(const char *onion_address);

// Extracts key from given onion domain to key memory location, function
// assumes that given onion address is valid
void onion_extract_key(const char *onion_address, uint8_t *key);

#endif