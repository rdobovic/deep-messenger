#include <stdio.h>
#include <onion.h>
#include <base32.h>
#include <stdint.h>
#include <helpers.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <debug.h>
#include <constants.h>
#include <sys_memory.h>
#include <sys_crash.h>

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
    EVP_DigestUpdate(mdctx, pub_key, ONION_PUB_KEY_LEN);
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

    for (i = 0; i < ONION_PUB_KEY_LEN; i++) {
        key[i] = onion_raw[i];
    }
}

// Generates onion address from given public key
void onion_address_from_pub_key(const uint8_t *pub_key, char *onion_address) {
    EVP_MD_CTX *ctx;
    int coded_len;
    uint8_t hash[EVP_MAX_MD_SIZE];
    int hash_len = EVP_MAX_MD_SIZE;
    uint8_t onion_version = ONION_VERSION;
    uint8_t buffer[ONION_PUB_KEY_LEN + ONION_CHECKSUM_LEN + 1];

    memcpy(buffer, pub_key, ONION_PUB_KEY_LEN);

    ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha3_256(), NULL);
    EVP_DigestUpdate(ctx, ".onion checksum", 15);
    EVP_DigestUpdate(ctx, pub_key, ONION_PUB_KEY_LEN);
    EVP_DigestUpdate(ctx, &onion_version, 1);
    EVP_DigestFinal_ex(ctx, hash, &hash_len);

    memcpy(buffer + ONION_PUB_KEY_LEN, hash, ONION_CHECKSUM_LEN);
    buffer[ONION_CHECKSUM_LEN + ONION_PUB_KEY_LEN] = onion_version;

    coded_len = base32_encode(buffer, sizeof(buffer), onion_address, 0);
    strcpy(onion_address + coded_len, ".onion");
}

// Expand given private key into a + RH format
void onion_priv_key_expand(const uint8_t *priv_key, uint8_t *expanded) {
    int len = 64;
    EVP_MD_CTX *ctx;

    ctx = EVP_MD_CTX_new();

    if (
        !EVP_DigestInit_ex2(ctx, EVP_sha512(), NULL) ||
        !EVP_DigestUpdate(ctx, priv_key, ED25519_PRIV_KEY_LEN) ||
        !EVP_DigestFinal(ctx, expanded, &len)
    ) {
        sys_crash("Openssl", "Failed to hash while expanding tor key, with error: %s",
            ERR_error_string(ERR_get_error(), NULL));
        return;
    }

    EVP_MD_CTX_free(ctx);

    expanded[0] &= 248;
    expanded[31] &= 127;
    expanded[31] |= 64;
}

// Convert public key into format stored in tor hidden service file
void onion_pub_key_hs_encode(const uint8_t *pub_key, uint8_t *encoded) {
    memcpy(encoded, "== ed25519v1-public: type0 ==\x00\x00\x00", 32);
    memcpy(encoded + 32, pub_key, ONION_PUB_KEY_LEN);
}

// Convert private key into format stored in tor hidden service file
void onion_priv_key_hs_encode(const uint8_t *priv_key, uint8_t *encoded) {
    memcpy(encoded, "== ed25519v1-secret: type0 ==\x00\x00\x00", 32);
    onion_priv_key_expand(priv_key, encoded + 32);
}

// Create needed files for hidden service directory
int onion_init_dir(const char *dir_path, const uint8_t *pub_key, const uint8_t *priv_key) {
    char *file_path;
    int dir_path_len;
    FILE *fp_pub, *fp_priv, *fp_hostname;
    char address[ONION_ADDRESS_LEN + 2];
    uint8_t buffer[max(ONION_PRIV_KEY_HS_ENCODED_LEN, ONION_PUB_KEY_HS_ENCODED_LEN)];

    dir_path_len = strlen(dir_path);
    file_path = safe_malloc(sizeof(char) * (dir_path_len + 32), "Failed to allocate char array");

    snprintf(file_path, dir_path_len + 32, "%s/hostname", dir_path);
    fp_hostname = fopen(file_path, "w");

    snprintf(file_path, dir_path_len + 32, "%s/hs_ed25519_public_key", dir_path);
    fp_pub = fopen(file_path, "w");

    snprintf(file_path, dir_path_len + 32, "%s/hs_ed25519_secret_key", dir_path);
    fp_priv = fopen(file_path, "w");

    if (!fp_hostname || !fp_pub || !fp_priv)
        return 0;

    onion_pub_key_hs_encode(pub_key, buffer);
    fwrite(buffer, sizeof(uint8_t), ONION_PUB_KEY_HS_ENCODED_LEN, fp_pub);

    onion_priv_key_hs_encode(priv_key, buffer);
    fwrite(buffer, sizeof(uint8_t), ONION_PRIV_KEY_HS_ENCODED_LEN, fp_priv);

    onion_address_from_pub_key(pub_key, address);
    address[ONION_ADDRESS_LEN] = '\n';
    fwrite(address, sizeof(uint8_t), ONION_ADDRESS_LEN + 1, fp_hostname);

    free(file_path);
    fclose(fp_pub);
    fclose(fp_priv);
    fclose(fp_hostname);
    return 1;
}