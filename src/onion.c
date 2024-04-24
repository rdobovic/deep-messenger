#include <onion.h>
#include <base32.h>
#include <stdint.h>
#include <helpers.h>
#include <openssl/evp.h>
#include <debug.h>

/**
 * Functions below are used to check and decode .onion address
 * more information on onion address encoding and checksum can be found
 * in tor spec:
 * 
 * https://spec.torproject.org/rend-spec/overview.html#cryptography
 * https://spec.torproject.org/rend-spec/encoding-onion-addresses.html
 * 
 */

// Checks if onion domain is valid
int onion_address_valid(const char *onion_address) {
    int i;
    size_t len;

    EVP_MD_CTX *mdctx;

    int hash_len;
    uint8_t hash[EVP_MAX_MD_SIZE];
    uint8_t onion_version = ONION_VERSION;

    uint8_t *pub_key, *checksum, version;
    uint8_t onion_raw[BASE32_DECODED_LEN(ONION_BASE_LEN)];

    const char onion_end[] = ".onion";

    for (i = 0; i < ONION_ADDRESS_LEN - ONION_BASE_LEN; i++) {
        if (onion_address[i + ONION_BASE_LEN] != onion_end[i])
            return 0;
    }

    if (!base32_valid(onion_address, ONION_BASE_LEN))
        return 0;

    len = base32_decode(onion_address, ONION_BASE_LEN, onion_raw);

    pub_key = onion_raw;
    version = onion_raw[len - 1];
    checksum = &onion_raw[len - 3];

    if (version != ONION_VERSION)
        return 0;

    mdctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex(mdctx, EVP_sha3_256(), NULL);
    EVP_DigestUpdate(mdctx, ".onion checksum", 15);
    EVP_DigestUpdate(mdctx, pub_key, ONION_KEY_LEN);
    EVP_DigestUpdate(mdctx, &onion_version, 1);
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);

    EVP_MD_CTX_free(mdctx);

    for (i = 0; i < ONION_CHECKSUM_LEN; i++) {
        if (hash[i] != checksum[i])
            return 0;
    }

    return 1;
}

// Extracts key from given onion domain to key memory location, function
// assumes that given onion address is valid
void onion_extract_key(const char *onion_address, uint8_t *key) {
    int i;
    uint8_t onion_raw[BASE32_DECODED_LEN(ONION_BASE_LEN)];

    base32_decode(onion_address, ONION_BASE_LEN, onion_raw);

    for (i = 0; i < ONION_KEY_LEN; i++) {
        key[i] = onion_raw[i];
    }

    return 1;
}