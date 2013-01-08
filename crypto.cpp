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

#include "crypto.h"

Crypto::Crypto() {
    _ready = false;
    _dh = DH_new();
    _pub_key = BN_new();
    EVP_CIPHER_CTX_init(&_encryption_ctx);
    EVP_CIPHER_CTX_init(&_decryption_ctx);
    HMAC_CTX_init(&_hmac_ctx);
}

Crypto::~Crypto() {
    DH_free(_dh);
    BN_free(_pub_key);
    EVP_CIPHER_CTX_cleanup(&_encryption_ctx);
    EVP_CIPHER_CTX_cleanup(&_decryption_ctx);
    HMAC_CTX_cleanup(&_hmac_ctx);
    memset(_key, 0, __key_size);
}

void Crypto::get_prime(block_t &dest) {
    DH_generate_parameters_ex(_dh, __key_length, 5, NULL);
    dest = block_t(block_t::data, BN_num_bytes(_dh->p));
    BN_bn2bin(_dh->p, dest._data);
}

void Crypto::get_public_key(block_t &dest) {
    DH_generate_key(_dh);
    dest = block_t(block_t::data, BN_num_bytes(_dh->pub_key));
    BN_bn2bin(_dh->pub_key, dest._data);
}

void Crypto::get_init_vector(block_t &dest) {
    dest = block_t(block_t::data, __iv_size);
    RAND_bytes(dest._data, __iv_size);
}

void Crypto::set_prime(const block_t &source) {
    DH_generate_parameters_ex(_dh, __key_length, 5, NULL);
    BN_bin2bn(source._data, source._size, _dh->p);
}

void Crypto::set_public_key(const block_t &source) {
    BN_bin2bn(source._data, source._size, _pub_key);
    DH_compute_key(_key, _pub_key, _dh);
    SHA256_Init(&_sha256_ctx);
    SHA256_Update(&_sha256_ctx, _key, __key_size);
    SHA256_Final(_key, &_sha256_ctx);
}

void Crypto::set_init_vector(const block_t &source) {
    memcpy(_iv, source._data, __iv_size);
    _ready = true;
}

void Crypto::encrypt_block(block_t &dest, const block_t &source) {

    int padding;

    dest._cmd = source._cmd;
    EVP_EncryptInit_ex(&_encryption_ctx, EVP_aes_256_cbc(), NULL, _key, _iv);
    EVP_EncryptUpdate(&_encryption_ctx, dest._data, &dest._size, source._data, source._size);
    EVP_EncryptFinal_ex(&_encryption_ctx, dest._data + dest._size, &padding);
    memcpy(_iv, _encryption_ctx.iv, __iv_size);
    dest._size += padding;
    HMAC_Init_ex(&_hmac_ctx, _key, __key_size, EVP_sha1(), NULL);
    HMAC_Update(&_hmac_ctx, dest._data, dest._size);
    HMAC_Final(&_hmac_ctx, dest._data + dest._size, NULL);
    dest._size += __hmac_size;
}

void Crypto::decrypt_block(block_t &dest, const block_t &source) {

    int padding, data_size = source._size - __hmac_size;
    unsigned char hmac[__hmac_size];

    HMAC_Init_ex(&_hmac_ctx, _key, __key_size, EVP_sha1(), NULL);
    HMAC_Update(&_hmac_ctx, source._data, data_size);
    HMAC_Final(&_hmac_ctx, hmac, NULL);
    if (memcmp(hmac, source._data + data_size, __hmac_size))
        throw std::runtime_error("can't authenticate block");
    dest._cmd = source._cmd;
    EVP_DecryptInit_ex(&_decryption_ctx, EVP_aes_256_cbc(), NULL, _key, _iv);
    EVP_DecryptUpdate(&_decryption_ctx, dest._data, &dest._size, source._data, data_size);
    EVP_DecryptFinal_ex(&_decryption_ctx, dest._data + dest._size, &padding);
    memcpy(_iv, _decryption_ctx.iv, __iv_size);
    dest._size += padding;
}
