# Svi korišteni izvori

Ovaj dokument sadrži listu svih izvora korištenih pri izradi deep messenger aplikacije.

## OpenSSL

Funkcije za kriptografske operacije u aplikaciji.

### Općenito

libcrypto in general<br>
https://www.openssl.org/docs/man3.0/man7/crypto.html

evp API in general<br>
https://www.openssl.org/docs/man3.0/man7/evp.html

EVP_aes_256_cbc()<br>
https://www.openssl.org/docs/man3.0/man3/EVP_aes_256_cbc.html

EVP_sha512<br>
https://www.openssl.org/docs/manmaster/man3/EVP_sha512.html

### Seal/Open letter

EVP_SealInit<br>
https://www.openssl.org/docs/manmaster/man3/EVP_SealInit.html

EVP_SealFinal (aka EVP_EncryptFinal)<br>
EVP_SealUpdate (aka EVP_EncryptUpdate)<br>
https://www.openssl.org/docs/manmaster/man3/EVP_EncryptInit.html

EVP_OpenInit<br>
https://www.openssl.org/docs/man3.0/man3/EVP_OpenInit.html

EVP_OpenFinal (aka EVP_DecryptFinal)<br>
EVP_OpenUpdate (aka EVP_DecryptUpdate)<br>
https://www.openssl.org/docs/man3.0/man3/EVP_EncryptInit.html

### Generate RSA keypair

EVP_RSA_gen<br>
https://www.openssl.org/docs/manmaster/man3/RSA_generate_key_ex.html

### Encode/Decode DER/PAM

OSSL_ENCODER_CTX_new_for_pkey<br>
https://www.openssl.org/docs/man3.0/man3/OSSL_ENCODER_CTX_new_for_pkey.html

OSSL_ENCODER_CTX<br>
https://www.openssl.org/docs/manmaster/man3/OSSL_ENCODER_CTX.html

OSSL_ENCODER_to_data<br>
https://www.openssl.org/docs/manmaster/man3/OSSL_ENCODER_to_bio.html

[int selection] attribute in both decoder and encoder values<br>
https://www.openssl.org/docs/man3.0/man3/EVP_PKEY_fromdata.html#Selections

OSSL_DECODER_CTX_new_for_pkey<br>
https://www.openssl.org/docs/manmaster/man3/OSSL_DECODER_CTX_new_for_pkey.html

OSSL_DECODER_CTX<br>
https://www.openssl.org/docs/manmaster/man3/OSSL_DECODER_CTX.html

OSSL_DECODER_from_data<br>
https://www.openssl.org/docs/manmaster/man3/OSSL_DECODER_from_bio.html

### Generate ED25519 keypair

EVP_PKEY_keygen<br>
https://www.openssl.org/docs/man3.0/man3/EVP_PKEY_Q_keygen.html

EVP_PKEY_CTX_new<br>
EVP_PKEY_CTX_new_id<br>
https://www.openssl.org/docs/man3.0/man3/EVP_PKEY_CTX_new.html

EVP_PKEY<br>
https://www.openssl.org/docs/man3.0/man3/EVP_PKEY.html

### ED25519 signing/verifying

EVP_DigestSign*<br>
https://www.openssl.org/docs/man3.0/man3/EVP_DigestSignInit.html

EVP_DigestVerify*<br>
https://www.openssl.org/docs/man3.0/man3/EVP_DigestVerifyInit.html

### Random bytes gen

https://www.openssl.org/docs/manmaster/man3/RAND_bytes.html

### SHA_3_256 hashing

https://www.openssl.org/docs/manmaster/man3/EVP_sha3_256.html<br>
https://www.openssl.org/docs/manmaster/man3/EVP_MD_CTX_new.html<br>
https://www.openssl.org/docs/man3.0/man3/EVP_DigestInit_ex.html

## Queue

Implementacija queue-a (korištena za inspiraciju)<br>
https://github.com/kostakis/Generic-Queue

Članak o cirkularnim queue-ovima, ali na kraju se ta logika ne koristi<br>
https://www.programiz.com/dsa/circular-queue

## Base32 decodeing/encodeing

https://datatracker.ietf.org/doc/html/rfc4648#section-6<br>
https://github.com/mjg59/tpmtotp/blob/master/base32.c<br>
https://github.com/mjg59/tpmtotp/blob/master/base32.h

## TOR (The Onion Network)

Tor protokol<br>
https://spec.torproject.org

TOR onion address<br>
https://spec.torproject.org/rend-spec/encoding-onion-addresses.html<br>
https://spec.torproject.org/rend-spec/overview.html#cryptography

TOR private key file<br>
https://tor.stackexchange.com/questions/23965/how-to-get-the-hostname-from-hs-ed25519-secret-key

Is hashing then signing secure?<br>
https://crypto.stackexchange.com/questions/27277/more-efficient-and-just-as-secure-to-sign-message-hash-using-ed25519

