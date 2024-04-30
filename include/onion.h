#ifndef _INCLUDE_ONION_H_
#define _INCLUDE_ONION_H_

#include <stdint.h>

#define ONION_VERSION      3

#define ONION_PUB_KEY_LEN  32
#define ONION_PRIV_KEY_LEN 32

#define ONION_BASE_LEN     56
#define ONION_ADDRESS_LEN  62 // 56 (address) + 1 (dot) + 5 (onion text)
#define ONION_CHECKSUM_LEN 2

#define ONION_PRIV_KEY_EXPANDED_LEN   64
#define ONION_PUB_KEY_HS_ENCODED_LEN  64
#define ONION_PRIV_KEY_HS_ENCODED_LEN 96

// Checks if onion domain is valid
int onion_address_valid(const char *onion_address);

// Extracts key from given onion domain to key memory location, function
// assumes that given onion address is valid
void onion_extract_key(const char *onion_address, uint8_t *key);

// Generates onion address from given public key
void onion_address_from_pub_key(const uint8_t *pub_key, char *onion_address);

// Expand given private key into a + RH format
void onion_priv_key_expand(const uint8_t *priv_key, uint8_t *expanded);

// Convert public key into format stored in tor hidden service file
void onion_pub_key_hs_encode(const uint8_t *pub_key, uint8_t *encoded);

// Convert private key into format stored in tor hidden service file
void onion_priv_key_hs_encode(const uint8_t *priv_key, uint8_t *encoded);

// Create needed files for hidden service directory
int onion_init_dir(const char *dir_path, const uint8_t *pub_key, const uint8_t *priv_key);

#endif