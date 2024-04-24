#include <onion.h>
#include <base32.h>
#include <stdint.h>
#include <helpers.h>
#include <openssl/evp.h>
#include <debug.h>


// Checks if onion domain is valid
int onion_address_valid(const char *onion_address) {
    int i;
    size_t len;

    EVP_MD *md;
    EVP_MD_CTX *mdctx;

    int hash_len;
    uint8_t hash[EVP_MAX_MD_SIZE];
    uint8_t onion_version = ONION_VERSION;

    uint8_t *calc_checksum;
    uint8_t *pub_key, *checksum, version;
    uint8_t onion_raw[BASE32_DECODED_LEN(ONION_BASE_LEN)];

    const char onion_end[] = ".onion";

    for (i = 0; i < ONION_ADDRESS_LEN - ONION_BASE_LEN; i++) {
        if (onion_address[i + ONION_BASE_LEN] != onion_end[i])
            return 0;
    }

    debug(".onion check passed !");

    if (!base32_valid(onion_address, ONION_BASE_LEN))
        return 0;

    debug("base32 check passed !");

    len = base32_decode(onion_address, ONION_BASE_LEN, onion_raw);

    debug("OUT LEN: %d", len);

    pub_key = onion_raw;
    version = onion_raw[len - 1];
    checksum = &onion_raw[len - 3];

    debug("PUB: %x...%x, VER: %x, CS: %x%x", pub_key[0], pub_key[ONION_KEY_LEN - 1], version, checksum[0], checksum[1]);

    if (version != ONION_VERSION)
        return 0;

    debug("Calculating hash");

    mdctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex(mdctx, EVP_sha3_256(), NULL);
    EVP_DigestUpdate(mdctx, ".onion checksum", 15);
    EVP_DigestUpdate(mdctx, pub_key, ONION_KEY_LEN);
    EVP_DigestUpdate(mdctx, &onion_version, 1);
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);

    EVP_MD_CTX_free(mdctx);

    debug("Hash len; %d", hash_len);

    debug("PK: %x%x", pub_key[ONION_KEY_LEN - 2], pub_key[ONION_KEY_LEN - 1]);

    calc_checksum = &hash[hash_len - ONION_CHECKSUM_LEN];

    debug("cs: %x%x = %x%x", checksum[0], checksum[1], hash[0], hash[1]);
    debug("cs: %x%x = %x%x", checksum[0], checksum[1], calc_checksum[0], calc_checksum[1]);

    for (i = 0; i < ONION_CHECKSUM_LEN; i++) {
        if (hash[i] != checksum[i])
            return 0;
    }

    return 1;
}

// Extracts key from given onion domain on key location
int onion_extract_key(const char *onion_address, uint8_t *key) {

}