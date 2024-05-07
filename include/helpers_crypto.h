#ifndef _INCLUDE_HELPERS_CRYPTO_H_
#define _INCLUDE_HELPERS_CRYPTO_H_

#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <sys_crash.h>

// Crash with openssl error message
#define sys_openssl_crash(desc) \
    sys_crash("Openssl", "%s, with error: %s", (desc), ERR_error_string(ERR_get_error(), NULL))

// Generate ED25519 keypair and place keys on given locations
void ed25519_keygen(uint8_t *public_key, uint8_t *private_key);

// Generate 2048bit RSA keypair, encode them in DER format and store keys
// to given locations, key lengths are defined by macros
void rsa_2048bit_keygen(uint8_t *public_key, uint8_t *private_key);

// Decodes given RSA public key encoded in DER format and returns pointer to
// EVP_PKEY on success or NULL on failure
EVP_PKEY * rsa_2048bit_pub_key_decode(uint8_t *public_key);

// Decodes given RSA private key encoded in DER format and returns pointer to
// EVP_PKEY on success or NULL on failure
EVP_PKEY * rsa_2048bit_priv_key_decode(uint8_t *private_key);

#endif