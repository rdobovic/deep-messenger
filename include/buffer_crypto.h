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

#endif