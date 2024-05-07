#ifndef _INCLUDE_BUFFER_CRYPTO_H_
#define _INCLUDE_BUFFER_CRYPTO_H_

#include <stdlib.h>
#include <stdint.h>
#include <event2/buffer.h>

// Take first len bytes in the given buffer, sign them using provided ed25519
// private key and add signature to the end of buffer
int ed25519_buffer_sign(struct evbuffer *buff, size_t len, uint8_t *priv_key);

// Takes last ED25519_SIGNATURE_LEN bytes as a ed25519 signature and validates
// it against other data using provided ed25519 public key
int ed25519_buffer_validate(struct evbuffer *buff, size_t len, uint8_t *pub_key);

// Takes RSA 2048bit key in DER format encrypts content of plain buffer and puts it into
// enc buffer in following format, used for sending it over network
//
//  >> DATA LEN (4 bytes)
//  >> DATA
//  >> DATA KEY (AES 32 bytes)
//  >> DATA IV  (16 bytes)
//
// returns 1 on success and 0 on failure
int rsa_buffer_encrypt(struct evbuffer *plain, uint8_t *der_pub_key, struct evbuffer *enc);

// Takes buffer encrypted by rsa_buffer_encrypt function and decrypts it into plain buffer
// using provided RSA 2048bit key in DER format, returns 1 on success and 0 on failure
int rsa_buffer_decrypt(struct evbuffer *enc, uint8_t *der_priv_key, struct evbuffer *plain);

#endif