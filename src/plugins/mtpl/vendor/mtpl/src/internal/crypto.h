#ifndef MTPL_INTERNAL_CRYPTO_H
#define MTPL_INTERNAL_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#include <mtpl/status.h>
#include <mtpl/types.h>

typedef struct mtpl_derived_key {
    uint8_t *data;
    size_t size;
} mtpl_derived_key_t;

typedef enum mtpl_compression_method {
    MTPL_COMPRESSION_NONE = 1,
    MTPL_COMPRESSION_SNAPPY = 2,
    MTPL_COMPRESSION_ZLIB = 3
} mtpl_compression_method_t;

mtpl_status_t mtpl_crypto_derive_key(
    const mtpl_crypto_options_t *options,
    mtpl_derived_key_t *key);

void mtpl_crypto_key_release(mtpl_derived_key_t *key);

mtpl_status_t mtpl_crypto_encrypt_frame(
    const mtpl_derived_key_t *key,
    mtpl_buffer_view_t input,
    mtpl_compression_method_t method,
    mtpl_buffer_t *output);

mtpl_status_t mtpl_crypto_decrypt_frame(
    const mtpl_derived_key_t *key,
    mtpl_buffer_view_t input,
    size_t output_size_limit,
    mtpl_buffer_t *output);

#endif
