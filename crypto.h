/*
  Copyright Xphysics 2012. All Rights Reserved.

  SafeChat is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  SafeChat is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE. See the GNU General Public License for more details.

  <http://www.gnu.org/licenses/>
 */

#ifndef crypto_h
#define	crypto_h

#include <stdexcept>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include "block.h"

#define __key_length    256
#define __key_size      __key_length / 8
#define __iv_size       AES_BLOCK_SIZE
#define __hmac_size     SHA_DIGEST_LENGTH
#define __data_size     __block_size - AES_BLOCK_SIZE - __hmac_size

class Crypto {
public:

    Crypto();
    ~Crypto();

    void get_prime(block_t &dest);
    void get_public_key(block_t &dest);
    void get_init_vector(block_t &dest);

    void set_prime(const block_t &source);
    void set_public_key(const block_t &source);
    void set_init_vector(const block_t &source);

    void encrypt_block(block_t &dest, const block_t &source);
    void decrypt_block(block_t &dest, const block_t &source);

    bool is_ready() const {
        return _ready;
    }

private:

    bool _ready;
    unsigned char _key[__key_size], _iv[__iv_size];
    DH *_dh;
    BIGNUM *_pub_key;
    SHA256_CTX _sha256_ctx;
    EVP_CIPHER_CTX _encryption_ctx, _decryption_ctx;
    HMAC_CTX _hmac_ctx;

};

#endif
