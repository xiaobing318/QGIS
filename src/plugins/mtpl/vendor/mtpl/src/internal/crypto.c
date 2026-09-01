#include "crypto.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include <mtpl/mtpl.h>

#include "arc4.h"
#include "base64_decode.h"
#include "csnappy.h"

#define MTPL_CRYPTO_FRAME_MAGIC "dTd"
#define MTPL_CRYPTO_FRAME_VERSION 1u
#define MTPL_CRYPTO_XOR_LIMIT 1024u
#define MTPL_CRYPTO_HEADER_SIZE 20u
#define MTPL_CRYPTO_XOR_OFFSET 18u

typedef struct mtpl_crypto_frame_header {
    char magic[4];
    uint16_t data_version;
    uint16_t compression_method;
    uint32_t original_size;
    uint32_t compressed_size;
    uint16_t xor_size;
    uint16_t data_offset;
} mtpl_crypto_frame_header_t;

_Static_assert(sizeof(mtpl_crypto_frame_header_t) == MTPL_CRYPTO_HEADER_SIZE,
    "Unexpected crypto frame header layout");

static void mtpl_crypto_zero_output(mtpl_buffer_t *output) {
    if (output != NULL) {
        output->data = NULL;
        output->size = 0;
    }
}

static bool mtpl_crypto_key_is_valid(const mtpl_derived_key_t *key) {
    return key != NULL && key->data != NULL && key->size > 0 && key->size <= INT_MAX;
}

mtpl_status_t mtpl_crypto_derive_key(
    const mtpl_crypto_options_t *options,
    mtpl_derived_key_t *key) {
    base64_decodestate decode_state;
    uint8_t *decoded = NULL;
    uint8_t *derived = NULL;
    uint32_t decoded_size;
    size_t index;
    uint32_t total = 0;
    uint8_t mask;

    if (key == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    key->data = NULL;
    key->size = 0;

    if (options == NULL || options->private_key == NULL ||
        options->device_key == NULL || options->private_key_size == 0 ||
        options->device_key_size == 0) {
        return MTPL_STATUS_KEY_REQUIRED;
    }
    if (options->private_key_size > UINT32_MAX || options->device_key_size > UINT32_MAX) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }

    decoded = (uint8_t *)malloc(options->private_key_size + 1);
    if (decoded == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memset(decoded, 0, options->private_key_size + 1);
    base64_init_decodestate(&decode_state);
    decoded_size = base64_decode_block(
        (const char *)options->private_key,
        (uint32_t)options->private_key_size,
        (char *)decoded,
        &decode_state);
    if (decoded_size == 0) {
        free(decoded);
        return MTPL_STATUS_CRYPTO_ERROR;
    }

    derived = (uint8_t *)malloc((size_t)decoded_size);
    if (derived == NULL) {
        free(decoded);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(derived, decoded, decoded_size);
    mask = options->device_key[0];

    derived[decoded_size / 2u] = decoded[decoded_size / 2u] ^ mask;
    derived[decoded_size / 4u] = decoded[decoded_size / 4u] ^ mask;
    derived[decoded_size / 6u] = decoded[decoded_size / 6u] ^ mask;

    for (index = 0; index < options->device_key_size; ++index) {
        uint32_t key_index = options->device_key[index];
        total += key_index;
        key_index %= decoded_size;
        derived[key_index] = decoded[key_index] ^ mask;
    }

    {
        uint32_t c1 = ((255u * 255u - total) & 255u) % decoded_size;
        uint32_t c2 = ((255u * 255u - total) >> 8u) % decoded_size;
        uint32_t c3 = (total & 255u) % decoded_size;
        uint32_t c4 = (c1 + c2 + c3) % decoded_size;
        derived[c1] = decoded[c1] ^ mask;
        derived[c2] = decoded[c2] ^ mask;
        derived[c3] = decoded[c3] ^ mask;
        derived[c4] = decoded[c4] ^ mask;
    }

    free(decoded);
    key->data = derived;
    key->size = decoded_size;
    return MTPL_STATUS_OK;
}

void mtpl_crypto_key_release(mtpl_derived_key_t *key) {
    if (key == NULL) {
        return;
    }
    if (key->data != NULL) {
        memset(key->data, 0, key->size);
        free(key->data);
    }
    key->data = NULL;
    key->size = 0;
}

mtpl_status_t mtpl_crypto_encrypt_frame(
    const mtpl_derived_key_t *key,
    mtpl_buffer_view_t input,
    mtpl_compression_method_t method,
    mtpl_buffer_t *output) {
    mtpl_crypto_frame_header_t header;
    uint8_t *compressed = NULL;
    size_t compressed_capacity = 0;
    uint32_t compressed_size = 0;
    size_t data_offset;
    size_t output_size;
    stream_state stream;

    mtpl_crypto_zero_output(output);
    if (output == NULL || (input.data == NULL && input.size != 0)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_crypto_key_is_valid(key)) {
        return MTPL_STATUS_KEY_REQUIRED;
    }
    if (input.size > UINT32_MAX) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }

    if (method == MTPL_COMPRESSION_NONE) {
        compressed_capacity = input.size;
    } else if (method == MTPL_COMPRESSION_SNAPPY) {
        compressed_capacity = csnappy_max_compressed_length((uint32_t)input.size);
    } else if (method == MTPL_COMPRESSION_ZLIB) {
        compressed_capacity = compressBound((uLong)input.size);
    } else {
        return MTPL_STATUS_UNSUPPORTED;
    }
    if (compressed_capacity > UINT32_MAX) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }

    compressed = (uint8_t *)malloc(compressed_capacity == 0 ? 1 : compressed_capacity);
    if (compressed == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }

    if (method == MTPL_COMPRESSION_NONE) {
        if (input.size != 0) {
            memcpy(compressed, input.data, input.size);
        }
        compressed_size = (uint32_t)input.size;
    } else if (method == MTPL_COMPRESSION_SNAPPY) {
        void *working_memory = malloc(1u << 15);
        if (working_memory == NULL) {
            free(compressed);
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        csnappy_compress(
            (const char *)input.data,
            (uint32_t)input.size,
            (char *)compressed,
            &compressed_size,
            working_memory,
            15);
        free(working_memory);
    } else {
        uLongf zlib_size = (uLongf)compressed_capacity;
        if (compress2(compressed, &zlib_size, input.data, (uLong)input.size,
                Z_DEFAULT_COMPRESSION) != Z_OK) {
            free(compressed);
            return MTPL_STATUS_COMPRESSION_ERROR;
        }
        if (zlib_size > UINT32_MAX) {
            free(compressed);
            return MTPL_STATUS_LIMIT_EXCEEDED;
        }
        compressed_size = (uint32_t)zlib_size;
    }

    data_offset = MTPL_CRYPTO_HEADER_SIZE + input.size % 16u;
    if (compressed_size > SIZE_MAX - data_offset) {
        free(compressed);
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    output_size = data_offset + compressed_size;
    output->data = (uint8_t *)calloc(output_size == 0 ? 1 : output_size, 1);
    if (output->data == NULL) {
        free(compressed);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }

    memset(&header, 0, sizeof(header));
    memcpy(header.magic, MTPL_CRYPTO_FRAME_MAGIC, sizeof(header.magic));
    header.data_version = MTPL_CRYPTO_FRAME_VERSION;
    header.compression_method = (uint16_t)method;
    header.original_size = (uint32_t)input.size;
    header.compressed_size = compressed_size;
    header.xor_size = (uint16_t)(compressed_size > MTPL_CRYPTO_XOR_LIMIT
        ? MTPL_CRYPTO_XOR_LIMIT
        : compressed_size);
    header.data_offset = (uint16_t)data_offset;
    memcpy(output->data, &header, sizeof(header));
    if (compressed_size != 0) {
        memcpy(output->data + data_offset, compressed, compressed_size);
    }
    free(compressed);

    stream_init(&stream, key->data, (int)key->size);
    stream_encrypt(&stream, output->data + MTPL_CRYPTO_XOR_OFFSET, header.xor_size);
    output->size = output_size;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_crypto_decrypt_frame(
    const mtpl_derived_key_t *key,
    mtpl_buffer_view_t input,
    size_t output_size_limit,
    mtpl_buffer_t *output) {
    mtpl_crypto_frame_header_t header;
    uint8_t *mutable_frame = NULL;
    const uint8_t *compressed;
    stream_state stream;
    size_t allocation_size;

    mtpl_crypto_zero_output(output);
    if (output == NULL || input.data == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_crypto_key_is_valid(key)) {
        return MTPL_STATUS_KEY_REQUIRED;
    }
    if (input.size < MTPL_CRYPTO_HEADER_SIZE) {
        return MTPL_STATUS_CORRUPT_DATA;
    }

    memcpy(&header, input.data, sizeof(header));
    if (memcmp(header.magic, MTPL_CRYPTO_FRAME_MAGIC, sizeof(header.magic)) != 0) {
        return MTPL_STATUS_FORMAT_ERROR;
    }
    if (header.data_version != MTPL_CRYPTO_FRAME_VERSION) {
        return MTPL_STATUS_UNSUPPORTED_VERSION;
    }
    if (header.xor_size > MTPL_CRYPTO_XOR_LIMIT ||
        (size_t)MTPL_CRYPTO_XOR_OFFSET + header.xor_size > input.size) {
        return MTPL_STATUS_CORRUPT_DATA;
    }

    mutable_frame = (uint8_t *)malloc(input.size);
    if (mutable_frame == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(mutable_frame, input.data, input.size);
    stream_init(&stream, key->data, (int)key->size);
    stream_encrypt(&stream, mutable_frame + MTPL_CRYPTO_XOR_OFFSET, header.xor_size);
    memcpy(&header, mutable_frame, sizeof(header));

    if (header.data_offset < MTPL_CRYPTO_HEADER_SIZE ||
        header.data_offset > input.size ||
        header.compressed_size > input.size - header.data_offset) {
        free(mutable_frame);
        return MTPL_STATUS_CORRUPT_DATA;
    }
    if (header.compression_method < MTPL_COMPRESSION_NONE ||
        header.compression_method > MTPL_COMPRESSION_ZLIB) {
        free(mutable_frame);
        return MTPL_STATUS_UNSUPPORTED;
    }
    if ((size_t)header.original_size > output_size_limit) {
        free(mutable_frame);
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }

    allocation_size = header.original_size == 0 ? 1 : header.original_size;
    output->data = (uint8_t *)malloc(allocation_size);
    if (output->data == NULL) {
        free(mutable_frame);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    compressed = mutable_frame + header.data_offset;

    if (header.compression_method == MTPL_COMPRESSION_NONE) {
        if (header.compressed_size != header.original_size) {
            mtpl_buffer_release(output);
            free(mutable_frame);
            return MTPL_STATUS_CORRUPT_DATA;
        }
        if (header.original_size != 0) {
            memcpy(output->data, compressed, header.original_size);
        }
    } else if (header.compression_method == MTPL_COMPRESSION_SNAPPY) {
        uint32_t expected_size = 0;
        if (csnappy_get_uncompressed_length(
                (const char *)compressed,
                header.compressed_size,
                &expected_size) < 0 ||
            expected_size != header.original_size ||
            csnappy_decompress(
                (const char *)compressed,
                header.compressed_size,
                (char *)output->data,
                header.original_size) != CSNAPPY_E_OK) {
            mtpl_buffer_release(output);
            free(mutable_frame);
            return MTPL_STATUS_DECOMPRESSION_ERROR;
        }
    } else {
        uLongf zlib_size = header.original_size;
        if (uncompress(
                output->data,
                &zlib_size,
                compressed,
                header.compressed_size) != Z_OK ||
            zlib_size != header.original_size) {
            mtpl_buffer_release(output);
            free(mutable_frame);
            return MTPL_STATUS_DECOMPRESSION_ERROR;
        }
    }

    output->size = header.original_size;
    free(mutable_frame);
    return MTPL_STATUS_OK;
}
