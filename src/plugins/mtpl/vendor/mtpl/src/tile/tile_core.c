#include "tile_core.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "../internal/crypto.h"
#include "../internal/file_io.h"

#define MTPL_TILE_MAGIC_SIZE 4u
#define MTPL_TILE_VERSION_PLAIN 0x1000u
#define MTPL_TILE_VERSION_ENCRYPTED 0x2000u
#define MTPL_TILE_INDEX_OFFSET_MASK UINT64_C(0xffffffffff)
#define MTPL_TILE_INDEX_SIZE_MAX UINT32_C(0x00ffffff)
#define MTPL_TILE_OFFSET_MAX UINT64_C(0xffffffffff)
#define MTPL_TILE_MAX_METADATA_SIZE (UINT32_C(256) * 1024u * 1024u)
#define MTPL_TILE_MAX_INDEX_BYTES (UINT64_C(512) * 1024u * 1024u)
#define MTPL_TILE_MAX_LOGICAL_SIZE (UINT32_C(256) * 1024u * 1024u)
#define MTPL_TILE_MAX_RANGE_COUNT UINT32_C(1000000)

typedef struct mtpl_tile_core_range {
    mtpl_tile_range_t bounds;
    size_t entry_count;
    uint64_t *entries;
    uint64_t index_file_offset;
} mtpl_tile_core_range_t;

struct mtpl_tile_core_reader {
    FILE *file;
    uint64_t file_size;
    char magic[MTPL_TILE_MAGIC_SIZE];
    mtpl_storage_mode_t mode;
    uint32_t tile_size;
    mtpl_buffer_t metadata;
    mtpl_tile_core_range_t *ranges;
    size_t range_count;
    mtpl_derived_key_t key;
};

struct mtpl_tile_core_writer {
    FILE *file;
    char magic[MTPL_TILE_MAGIC_SIZE];
    mtpl_storage_mode_t mode;
    uint32_t tile_size;
    mtpl_buffer_t compressed_metadata;
    mtpl_tile_core_range_t *ranges;
    size_t range_count;
    size_t range_capacity;
    uint64_t total_index_bytes;
    bool existing;
    bool data_started;
    mtpl_derived_key_t key;
};

static void mtpl_tile_zero_buffer(mtpl_buffer_t *buffer) {
    if (buffer != NULL) {
        buffer->data = NULL;
        buffer->size = 0;
    }
}

static bool mtpl_tile_size_is_supported(uint32_t tile_size) {
    return tile_size == 33u || tile_size == 129u || tile_size == 256u;
}

static mtpl_status_t mtpl_tile_file_open_status(void) {
    return errno == ENOENT ? MTPL_STATUS_FILE_NOT_FOUND : MTPL_STATUS_IO_ERROR;
}

static mtpl_status_t mtpl_tile_read_exact(FILE *file, void *data, size_t size) {
    uint8_t *output = (uint8_t *)data;
    size_t completed = 0u;

    while (completed < size) {
        size_t current = fread(output + completed, 1u, size - completed, file);
        if (current == 0u) {
            return ferror(file) != 0
                ? MTPL_STATUS_IO_ERROR
                : MTPL_STATUS_CORRUPT_DATA;
        }
        completed += current;
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_write_exact(
    FILE *file,
    const void *data,
    size_t size) {
    const uint8_t *input = (const uint8_t *)data;
    size_t completed = 0u;

    while (completed < size) {
        size_t current = fwrite(input + completed, 1u, size - completed, file);
        if (current == 0u) {
            return MTPL_STATUS_IO_ERROR;
        }
        completed += current;
    }
    return MTPL_STATUS_OK;
}

static uint32_t mtpl_tile_load_u32_le(const uint8_t data[4]) {
    return ((uint32_t)data[0]) |
        ((uint32_t)data[1] << 8u) |
        ((uint32_t)data[2] << 16u) |
        ((uint32_t)data[3] << 24u);
}

static uint64_t mtpl_tile_load_u64_le(const uint8_t data[8]) {
    return ((uint64_t)data[0]) |
        ((uint64_t)data[1] << 8u) |
        ((uint64_t)data[2] << 16u) |
        ((uint64_t)data[3] << 24u) |
        ((uint64_t)data[4] << 32u) |
        ((uint64_t)data[5] << 40u) |
        ((uint64_t)data[6] << 48u) |
        ((uint64_t)data[7] << 56u);
}

static void mtpl_tile_store_u32_le(uint8_t data[4], uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void mtpl_tile_store_u64_le(uint8_t data[8], uint64_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
    data[4] = (uint8_t)(value >> 32u);
    data[5] = (uint8_t)(value >> 40u);
    data[6] = (uint8_t)(value >> 48u);
    data[7] = (uint8_t)(value >> 56u);
}

static mtpl_status_t mtpl_tile_read_u32(FILE *file, uint32_t *value) {
    uint8_t encoded[4];
    mtpl_status_t status;
    if (file == NULL || value == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_read_exact(file, encoded, sizeof(encoded));
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    *value = mtpl_tile_load_u32_le(encoded);
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_read_u64(FILE *file, uint64_t *value) {
    uint8_t encoded[8];
    mtpl_status_t status;
    if (file == NULL || value == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_read_exact(file, encoded, sizeof(encoded));
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    *value = mtpl_tile_load_u64_le(encoded);
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_write_u32(FILE *file, uint32_t value) {
    uint8_t encoded[4];
    mtpl_tile_store_u32_le(encoded, value);
    return mtpl_tile_write_exact(file, encoded, sizeof(encoded));
}

static mtpl_status_t mtpl_tile_write_u64(FILE *file, uint64_t value) {
    uint8_t encoded[8];
    mtpl_tile_store_u64_le(encoded, value);
    return mtpl_tile_write_exact(file, encoded, sizeof(encoded));
}

static mtpl_status_t mtpl_tile_range_entry_count(
    const mtpl_tile_range_t *range,
    size_t *entry_count) {
    uint64_t width;
    uint64_t height;
    uint64_t count;

    if (range == NULL || entry_count == NULL ||
        range->x_min > range->x_max || range->y_min > range->y_max) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    width = (uint64_t)range->x_max - range->x_min + 1u;
    height = (uint64_t)range->y_max - range->y_min + 1u;
    if (width > UINT64_MAX / height) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    count = width * height;
    if (count > SIZE_MAX / sizeof(uint64_t) ||
        count * sizeof(uint64_t) > MTPL_TILE_MAX_INDEX_BYTES) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    *entry_count = (size_t)count;
    return MTPL_STATUS_OK;
}

static bool mtpl_tile_ranges_overlap(
    const mtpl_tile_range_t *left,
    const mtpl_tile_range_t *right) {
    return left->zoom == right->zoom &&
        left->x_min <= right->x_max && right->x_min <= left->x_max &&
        left->y_min <= right->y_max && right->y_min <= left->y_max;
}

static void mtpl_tile_ranges_release(
    mtpl_tile_core_range_t *ranges,
    size_t range_count) {
    size_t index;
    if (ranges == NULL) {
        return;
    }
    for (index = 0; index < range_count; ++index) {
        free(ranges[index].entries);
    }
    free(ranges);
}

static void mtpl_tile_reader_destroy(mtpl_tile_core_reader_t *reader) {
    if (reader == NULL) {
        return;
    }
    if (reader->file != NULL) {
        fclose(reader->file);
    }
    free(reader->metadata.data);
    mtpl_tile_ranges_release(reader->ranges, reader->range_count);
    mtpl_crypto_key_release(&reader->key);
    free(reader);
}

static void mtpl_tile_writer_destroy(mtpl_tile_core_writer_t *writer) {
    if (writer == NULL) {
        return;
    }
    if (writer->file != NULL) {
        fclose(writer->file);
    }
    free(writer->compressed_metadata.data);
    mtpl_tile_ranges_release(writer->ranges, writer->range_count);
    mtpl_crypto_key_release(&writer->key);
    free(writer);
}

static mtpl_status_t mtpl_tile_decompress_metadata(
    const uint8_t *compressed,
    size_t compressed_size,
    mtpl_buffer_t *metadata) {
    uint8_t *output;
    size_t capacity;

    mtpl_tile_zero_buffer(metadata);
    if (metadata == NULL || compressed == NULL || compressed_size == 0 ||
        compressed_size > UINT32_MAX) {
        return MTPL_STATUS_FORMAT_ERROR;
    }

    capacity = compressed_size > 64u ? compressed_size * 4u : 256u;
    if (capacity > MTPL_TILE_MAX_METADATA_SIZE) {
        capacity = MTPL_TILE_MAX_METADATA_SIZE;
    }
    output = (uint8_t *)malloc(capacity);
    if (output == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }

    for (;;) {
        uLongf output_size = (uLongf)capacity;
        int zlib_status = uncompress(
            output,
            &output_size,
            compressed,
            (uLong)compressed_size);
        if (zlib_status == Z_OK) {
            if (output_size == 0) {
                free(output);
                output = NULL;
            }
            metadata->data = output;
            metadata->size = (size_t)output_size;
            return MTPL_STATUS_OK;
        }
        if (zlib_status != Z_BUF_ERROR) {
            free(output);
            return MTPL_STATUS_DECOMPRESSION_ERROR;
        }
        if (capacity >= MTPL_TILE_MAX_METADATA_SIZE) {
            free(output);
            return MTPL_STATUS_LIMIT_EXCEEDED;
        }
        {
            size_t new_capacity = capacity > MTPL_TILE_MAX_METADATA_SIZE / 2u
                ? MTPL_TILE_MAX_METADATA_SIZE
                : capacity * 2u;
            uint8_t *resized = (uint8_t *)realloc(output, new_capacity);
            if (resized == NULL) {
                free(output);
                return MTPL_STATUS_OUT_OF_MEMORY;
            }
            output = resized;
            capacity = new_capacity;
        }
    }
}

static mtpl_status_t mtpl_tile_compress_metadata(
    mtpl_buffer_view_t metadata,
    mtpl_buffer_t *compressed) {
    static const uint8_t empty_input = 0;
    uLongf capacity;
    uLongf compressed_size;

    mtpl_tile_zero_buffer(compressed);
    if (compressed == NULL || (metadata.data == NULL && metadata.size != 0)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (metadata.size > MTPL_TILE_MAX_METADATA_SIZE || metadata.size > UINT32_MAX) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    capacity = compressBound((uLong)metadata.size);
    if (capacity > UINT32_MAX) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    compressed->data = (uint8_t *)malloc(capacity == 0 ? 1 : (size_t)capacity);
    if (compressed->data == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    compressed_size = capacity;
    if (compress2(
            compressed->data,
            &compressed_size,
            metadata.data == NULL ? &empty_input : metadata.data,
            (uLong)metadata.size,
            Z_DEFAULT_COMPRESSION) != Z_OK) {
        free(compressed->data);
        mtpl_tile_zero_buffer(compressed);
        return MTPL_STATUS_COMPRESSION_ERROR;
    }
    if (compressed_size > MTPL_TILE_MAX_METADATA_SIZE) {
        free(compressed->data);
        mtpl_tile_zero_buffer(compressed);
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    compressed->size = (size_t)compressed_size;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_reader_parse(
    mtpl_tile_core_reader_t *reader,
    const char expected_magic[4]) {
    uint32_t version;
    uint32_t ignored_reserve;
    uint32_t compressed_metadata_size = 0;
    uint32_t ignored_number_of_tiles;
    uint32_t range_count = 0;
    uint32_t index_offset;
    uint8_t *compressed_metadata = NULL;
    uint64_t current_offset;
    uint64_t total_index_bytes = 0;
    size_t range_index;
    mtpl_status_t status;

    status = mtpl_file_get_size(reader->file, &reader->file_size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (reader->file_size < MTPL_TILE_MAGIC_SIZE) {
        return MTPL_STATUS_FORMAT_ERROR;
    }
    status = mtpl_file_seek(reader->file, 0);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_tile_read_exact(
        reader->file,
        reader->magic,
        sizeof(reader->magic));
    if (status != MTPL_STATUS_OK) {
        return status == MTPL_STATUS_CORRUPT_DATA
            ? MTPL_STATUS_FORMAT_ERROR
            : status;
    }
    if (memcmp(reader->magic, expected_magic, MTPL_TILE_MAGIC_SIZE) != 0) {
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
    status = mtpl_tile_read_u32(reader->file, &version);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (version == MTPL_TILE_VERSION_PLAIN) {
        reader->mode = MTPL_STORAGE_PLAIN;
    } else if (version == MTPL_TILE_VERSION_ENCRYPTED) {
        reader->mode = MTPL_STORAGE_ENCRYPTED;
    } else {
        return MTPL_STATUS_UNSUPPORTED_VERSION;
    }
    status = mtpl_tile_read_u32(reader->file, &ignored_reserve);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_read_u32(reader->file, &compressed_metadata_size);
    }
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    (void)ignored_reserve;
    if (compressed_metadata_size == 0 ||
        compressed_metadata_size > MTPL_TILE_MAX_METADATA_SIZE ||
        compressed_metadata_size > reader->file_size) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    compressed_metadata = (uint8_t *)malloc(compressed_metadata_size);
    if (compressed_metadata == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    status = mtpl_tile_read_exact(
        reader->file,
        compressed_metadata,
        compressed_metadata_size);
    if (status != MTPL_STATUS_OK) {
        free(compressed_metadata);
        return status;
    }
    status = mtpl_tile_decompress_metadata(
        compressed_metadata,
        compressed_metadata_size,
        &reader->metadata);
    free(compressed_metadata);
    if (status != MTPL_STATUS_OK) {
        return status;
    }

    status = mtpl_tile_read_u32(reader->file, &reader->tile_size);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_read_u32(reader->file, &ignored_number_of_tiles);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_read_u32(reader->file, &range_count);
    }
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    (void)ignored_number_of_tiles;
    if (!mtpl_tile_size_is_supported(reader->tile_size)) {
        return MTPL_STATUS_FORMAT_ERROR;
    }
    if (range_count > MTPL_TILE_MAX_RANGE_COUNT ||
        (uint64_t)range_count * 20u > reader->file_size) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    if (range_count != 0) {
        reader->ranges = (mtpl_tile_core_range_t *)calloc(
            range_count,
            sizeof(*reader->ranges));
        if (reader->ranges == NULL) {
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
    }
    reader->range_count = range_count;

    for (range_index = 0; range_index < reader->range_count; ++range_index) {
        mtpl_tile_core_range_t *range = &reader->ranges[range_index];
        size_t prior_index;
        status = mtpl_tile_read_u32(reader->file, &range->bounds.zoom);
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_read_u32(reader->file, &range->bounds.x_min);
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_read_u32(reader->file, &range->bounds.x_max);
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_read_u32(reader->file, &range->bounds.y_min);
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_read_u32(reader->file, &range->bounds.y_max);
        }
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        status = mtpl_tile_range_entry_count(&range->bounds, &range->entry_count);
        if (status == MTPL_STATUS_INVALID_ARGUMENT) {
            return MTPL_STATUS_CORRUPT_DATA;
        }
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        if ((uint64_t)range->entry_count >
            (MTPL_TILE_MAX_INDEX_BYTES - total_index_bytes) / sizeof(uint64_t)) {
            return MTPL_STATUS_LIMIT_EXCEEDED;
        }
        total_index_bytes += (uint64_t)range->entry_count * sizeof(uint64_t);
        for (prior_index = 0; prior_index < range_index; ++prior_index) {
            if (mtpl_tile_ranges_overlap(
                    &reader->ranges[prior_index].bounds,
                    &range->bounds)) {
                return MTPL_STATUS_CORRUPT_DATA;
            }
        }
    }

    status = mtpl_tile_read_u32(reader->file, &index_offset);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_file_tell(reader->file, &current_offset);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if ((uint64_t)index_offset < current_offset ||
        (uint64_t)index_offset > reader->file_size ||
        total_index_bytes > reader->file_size - index_offset) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    status = mtpl_file_seek(reader->file, index_offset);
    if (status != MTPL_STATUS_OK) {
        return status;
    }

    for (range_index = 0; range_index < reader->range_count; ++range_index) {
        mtpl_tile_core_range_t *range = &reader->ranges[range_index];
        size_t entry_index;
        status = mtpl_file_tell(reader->file, &range->index_file_offset);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        range->entries = (uint64_t *)calloc(
            range->entry_count,
            sizeof(*range->entries));
        if (range->entries == NULL) {
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        for (entry_index = 0; entry_index < range->entry_count; ++entry_index) {
            status = mtpl_tile_read_u64(reader->file, &range->entries[entry_index]);
            if (status != MTPL_STATUS_OK) {
                return status;
            }
        }
    }

    status = mtpl_file_tell(reader->file, &current_offset);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    for (range_index = 0; range_index < reader->range_count; ++range_index) {
        mtpl_tile_core_range_t *range = &reader->ranges[range_index];
        size_t entry_index;
        for (entry_index = 0; entry_index < range->entry_count; ++entry_index) {
            uint64_t packed = range->entries[entry_index];
            uint64_t data_offset;
            uint32_t data_size;
            if (packed == 0) {
                continue;
            }
            data_offset = packed & MTPL_TILE_INDEX_OFFSET_MASK;
            data_size = (uint32_t)(packed >> 40u);
            if (data_size == 0 || data_offset < current_offset ||
                data_offset > reader->file_size ||
                data_size > reader->file_size - data_offset) {
                return MTPL_STATUS_CORRUPT_DATA;
            }
        }
    }
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_open(
    const char *path,
    const char expected_magic[4],
    const mtpl_crypto_options_t *crypto,
    mtpl_tile_core_reader_t **reader) {
    mtpl_tile_core_reader_t *opened;
    mtpl_status_t status;

    if (reader != NULL) {
        *reader = NULL;
    }
    if (path == NULL || path[0] == '\0' || expected_magic == NULL || reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_path_is_valid_utf8(path)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    opened = (mtpl_tile_core_reader_t *)calloc(1, sizeof(*opened));
    if (opened == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    opened->file = mtpl_fopen_utf8(path, "rb");
    if (opened->file == NULL) {
        status = mtpl_tile_file_open_status();
        mtpl_tile_reader_destroy(opened);
        return status;
    }
    status = mtpl_tile_reader_parse(opened, expected_magic);
    if (status != MTPL_STATUS_OK) {
        mtpl_tile_reader_destroy(opened);
        return status;
    }
    if (opened->mode == MTPL_STORAGE_ENCRYPTED && crypto != NULL) {
        status = mtpl_crypto_derive_key(crypto, &opened->key);
        if (status != MTPL_STATUS_OK) {
            mtpl_tile_reader_destroy(opened);
            return status;
        }
    }
    *reader = opened;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_find_reader_entry(
    mtpl_tile_core_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_tile_core_range_t **found_range,
    size_t *found_index) {
    size_t range_index;

    if (reader == NULL || coordinate == NULL ||
        found_range == NULL || found_index == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    for (range_index = 0; range_index < reader->range_count; ++range_index) {
        mtpl_tile_core_range_t *range = &reader->ranges[range_index];
        if (coordinate->zoom == range->bounds.zoom &&
            coordinate->x >= range->bounds.x_min &&
            coordinate->x <= range->bounds.x_max &&
            coordinate->y >= range->bounds.y_min &&
            coordinate->y <= range->bounds.y_max) {
            uint64_t width = (uint64_t)range->bounds.x_max - range->bounds.x_min + 1u;
            uint64_t entry_index =
                ((uint64_t)coordinate->y - range->bounds.y_min) * width +
                ((uint64_t)coordinate->x - range->bounds.x_min);
            if (entry_index >= range->entry_count) {
                return MTPL_STATUS_CORRUPT_DATA;
            }
            *found_range = range;
            *found_index = (size_t)entry_index;
            return MTPL_STATUS_OK;
        }
    }
    return MTPL_STATUS_OUT_OF_RANGE;
}

mtpl_status_t mtpl_tile_core_reader_read_tile_raw(
    mtpl_tile_core_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile) {
    mtpl_tile_core_range_t *range;
    size_t entry_index;
    uint64_t packed;
    uint64_t data_offset;
    uint32_t data_size;
    mtpl_status_t status;

    mtpl_tile_zero_buffer(tile);
    if (tile == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_find_reader_entry(
        reader,
        coordinate,
        &range,
        &entry_index);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    packed = range->entries[entry_index];
    if (packed == 0) {
        return MTPL_STATUS_NOT_FOUND;
    }
    data_offset = packed & MTPL_TILE_INDEX_OFFSET_MASK;
    data_size = (uint32_t)(packed >> 40u);
    if (data_size == 0 || data_offset > reader->file_size ||
        data_size > reader->file_size - data_offset) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    tile->data = (uint8_t *)malloc(data_size);
    if (tile->data == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    status = mtpl_file_seek(reader->file, data_offset);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_read_exact(reader->file, tile->data, data_size);
    }
    if (status != MTPL_STATUS_OK) {
        free(tile->data);
        mtpl_tile_zero_buffer(tile);
        return status;
    }
    tile->size = data_size;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_read_tile(
    mtpl_tile_core_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile) {
    mtpl_buffer_t raw;
    mtpl_status_t status;

    mtpl_tile_zero_buffer(tile);
    if (reader == NULL || tile == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_reader_read_tile_raw(reader, coordinate, &raw);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (reader->mode == MTPL_STORAGE_PLAIN) {
        *tile = raw;
        return MTPL_STATUS_OK;
    }
    if (reader->key.data == NULL || reader->key.size == 0) {
        free(raw.data);
        return MTPL_STATUS_KEY_REQUIRED;
    }
    if (raw.size >= 12u &&
        mtpl_tile_load_u32_le(raw.data + 8u) > MTPL_TILE_MAX_LOGICAL_SIZE) {
        free(raw.data);
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    status = mtpl_crypto_decrypt_frame(
        &reader->key,
        (mtpl_buffer_view_t){raw.data, raw.size},
        MTPL_TILE_MAX_LOGICAL_SIZE,
        tile);
    free(raw.data);
    if (status == MTPL_STATUS_FORMAT_ERROR ||
        status == MTPL_STATUS_CORRUPT_DATA ||
        status == MTPL_STATUS_DECOMPRESSION_ERROR) {
        return MTPL_STATUS_CRYPTO_ERROR;
    }
    return status;
}

mtpl_status_t mtpl_tile_core_reader_read_metadata(
    mtpl_tile_core_reader_t *reader,
    mtpl_buffer_t *metadata) {
    mtpl_tile_zero_buffer(metadata);
    if (reader == NULL || metadata == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (reader->metadata.size == 0) {
        return MTPL_STATUS_OK;
    }
    metadata->data = (uint8_t *)malloc(reader->metadata.size);
    if (metadata->data == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(metadata->data, reader->metadata.data, reader->metadata.size);
    metadata->size = reader->metadata.size;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_check_edge(
    mtpl_tile_core_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    uint8_t *edge) {
    mtpl_tile_core_range_t *range;
    size_t entry_index;
    size_t width;
    mtpl_status_t status;
    uint8_t value = 0;

    if (edge != NULL) {
        *edge = 0;
    }
    if (edge == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_find_reader_entry(
        reader,
        coordinate,
        &range,
        &entry_index);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (range->entries[entry_index] == 0) {
        return MTPL_STATUS_NOT_FOUND;
    }
    width = (size_t)((uint64_t)range->bounds.x_max - range->bounds.x_min + 1u);
    if (coordinate->y == range->bounds.y_min) {
        value |= 0x30u;
    } else if (range->entries[entry_index - width] == 0) {
        value |= 0x10u;
    }
    if (coordinate->y == range->bounds.y_max) {
        value |= 0x03u;
    } else if (range->entries[entry_index + width] == 0) {
        value |= 0x01u;
    }
    if (coordinate->x == range->bounds.x_min) {
        value |= 0xc0u;
    } else if (range->entries[entry_index - 1u] == 0) {
        value |= 0x40u;
    }
    if (coordinate->x == range->bounds.x_max) {
        value |= 0x0cu;
    } else if (range->entries[entry_index + 1u] == 0) {
        value |= 0x04u;
    }
    *edge = value;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_get_tile_size(
    mtpl_tile_core_reader_t *reader,
    uint32_t *tile_size) {
    if (tile_size == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *tile_size = 0u;
    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *tile_size = reader->tile_size;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_get_storage_mode(
    mtpl_tile_core_reader_t *reader,
    mtpl_storage_mode_t *mode) {
    if (mode == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *mode = MTPL_STORAGE_PLAIN;
    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *mode = reader->mode;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_get_range_count(
    const mtpl_tile_core_reader_t *reader,
    size_t *range_count) {
    if (range_count != NULL) {
        *range_count = 0u;
    }
    if (reader == NULL || range_count == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *range_count = reader->range_count;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_get_range_info(
    const mtpl_tile_core_reader_t *reader,
    size_t range_index,
    mtpl_tile_range_info_t *info) {
    const mtpl_tile_core_range_t *range;
    size_t slot_index;

    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }
    if (reader == NULL || info == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (range_index >= reader->range_count) {
        return MTPL_STATUS_OUT_OF_RANGE;
    }
    range = &reader->ranges[range_index];
    info->bounds = range->bounds;
    info->slot_count = range->entry_count;
    for (slot_index = 0u; slot_index < range->entry_count; ++slot_index) {
        if (range->entries[slot_index] != 0u) {
            ++info->present_count;
        }
    }
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_get_entry_info(
    const mtpl_tile_core_reader_t *reader,
    size_t range_index,
    size_t slot_index,
    mtpl_tile_entry_info_t *info) {
    const mtpl_tile_core_range_t *range;
    uint64_t width;
    uint64_t packed;

    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }
    if (reader == NULL || info == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (range_index >= reader->range_count) {
        return MTPL_STATUS_OUT_OF_RANGE;
    }
    range = &reader->ranges[range_index];
    if (slot_index >= range->entry_count) {
        return MTPL_STATUS_OUT_OF_RANGE;
    }
    width = (uint64_t)range->bounds.x_max - range->bounds.x_min + 1u;
    info->coordinate.zoom = range->bounds.zoom;
    info->coordinate.x = range->bounds.x_min +
        (uint32_t)((uint64_t)slot_index % width);
    info->coordinate.y = range->bounds.y_min +
        (uint32_t)((uint64_t)slot_index / width);
    packed = range->entries[slot_index];
    info->present = packed != 0u;
    if (info->present) {
        info->stored_size = packed >> 40u;
    }
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_reader_close(mtpl_tile_core_reader_t *reader) {
    mtpl_status_t status = MTPL_STATUS_OK;
    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (reader->file != NULL && fclose(reader->file) != 0) {
        status = MTPL_STATUS_IO_ERROR;
    }
    reader->file = NULL;
    mtpl_tile_reader_destroy(reader);
    return status;
}

static mtpl_status_t mtpl_tile_writer_write_header(
    mtpl_tile_core_writer_t *writer) {
    uint32_t version;
    uint64_t position;
    size_t range_index;
    mtpl_status_t status;

    if (writer == NULL || writer->file == NULL || writer->existing ||
        writer->range_count > UINT32_MAX ||
        writer->compressed_metadata.size > UINT32_MAX) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_file_seek(writer->file, 0);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_tile_write_exact(
        writer->file,
        writer->magic,
        sizeof(writer->magic));
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    version = writer->mode == MTPL_STORAGE_ENCRYPTED
        ? MTPL_TILE_VERSION_ENCRYPTED
        : MTPL_TILE_VERSION_PLAIN;
    status = mtpl_tile_write_u32(writer->file, version);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_write_u32(writer->file, 0);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_write_u32(
            writer->file,
            (uint32_t)writer->compressed_metadata.size);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_write_exact(
            writer->file,
            writer->compressed_metadata.data,
            writer->compressed_metadata.size);
    }
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_tile_write_u32(writer->file, writer->tile_size);
    if (status == MTPL_STATUS_OK) {
        /* The reference format leaves this legacy header field at zero. */
        status = mtpl_tile_write_u32(writer->file, 0);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_write_u32(writer->file, (uint32_t)writer->range_count);
    }
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    for (range_index = 0; range_index < writer->range_count; ++range_index) {
        const mtpl_tile_range_t *range = &writer->ranges[range_index].bounds;
        status = mtpl_tile_write_u32(writer->file, range->zoom);
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_write_u32(writer->file, range->x_min);
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_write_u32(writer->file, range->x_max);
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_write_u32(writer->file, range->y_min);
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_write_u32(writer->file, range->y_max);
        }
        if (status != MTPL_STATUS_OK) {
            return status;
        }
    }
    status = mtpl_file_tell(writer->file, &position);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (position > UINT32_MAX - sizeof(uint32_t)) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    status = mtpl_tile_write_u32(writer->file, (uint32_t)(position + sizeof(uint32_t)));
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    for (range_index = 0; range_index < writer->range_count; ++range_index) {
        mtpl_tile_core_range_t *range = &writer->ranges[range_index];
        size_t entry_index;
        status = mtpl_file_tell(writer->file, &range->index_file_offset);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        for (entry_index = 0; entry_index < range->entry_count; ++entry_index) {
            status = mtpl_tile_write_u64(writer->file, range->entries[entry_index]);
            if (status != MTPL_STATUS_OK) {
                return status;
            }
        }
    }
    return fflush(writer->file) == 0 ? MTPL_STATUS_OK : MTPL_STATUS_IO_ERROR;
}

static mtpl_status_t mtpl_tile_writer_write_existing_indices(
    mtpl_tile_core_writer_t *writer) {
    size_t range_index;
    for (range_index = 0; range_index < writer->range_count; ++range_index) {
        mtpl_tile_core_range_t *range = &writer->ranges[range_index];
        size_t entry_index;
        mtpl_status_t status = mtpl_file_seek(writer->file, range->index_file_offset);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        for (entry_index = 0; entry_index < range->entry_count; ++entry_index) {
            status = mtpl_tile_write_u64(writer->file, range->entries[entry_index]);
            if (status != MTPL_STATUS_OK) {
                return status;
            }
        }
    }
    return fflush(writer->file) == 0 ? MTPL_STATUS_OK : MTPL_STATUS_IO_ERROR;
}

mtpl_status_t mtpl_tile_core_writer_create(
    const char *path,
    const char magic[4],
    uint32_t tile_size,
    mtpl_buffer_view_t metadata,
    mtpl_storage_mode_t mode,
    const mtpl_crypto_options_t *crypto,
    mtpl_tile_core_writer_t **writer) {
    mtpl_tile_core_writer_t *created;
    mtpl_status_t status;

    if (writer != NULL) {
        *writer = NULL;
    }
    if (path == NULL || path[0] == '\0' || magic == NULL || writer == NULL ||
        !mtpl_tile_size_is_supported(tile_size) ||
        (metadata.data == NULL && metadata.size != 0) ||
        (mode != MTPL_STORAGE_PLAIN && mode != MTPL_STORAGE_ENCRYPTED)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_path_is_valid_utf8(path)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    created = (mtpl_tile_core_writer_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(created->magic, magic, sizeof(created->magic));
    created->tile_size = tile_size;
    created->mode = mode;
    status = mtpl_tile_compress_metadata(metadata, &created->compressed_metadata);
    if (status != MTPL_STATUS_OK) {
        mtpl_tile_writer_destroy(created);
        return status;
    }
    if (mode == MTPL_STORAGE_ENCRYPTED) {
        status = mtpl_crypto_derive_key(crypto, &created->key);
        if (status != MTPL_STATUS_OK) {
            mtpl_tile_writer_destroy(created);
            return status;
        }
    }
    created->file = mtpl_fopen_utf8(path, "w+b");
    if (created->file == NULL) {
        status = mtpl_tile_file_open_status();
        mtpl_tile_writer_destroy(created);
        return status;
    }
    status = mtpl_tile_writer_write_header(created);
    if (status != MTPL_STATUS_OK) {
        mtpl_tile_writer_destroy(created);
        return status;
    }
    *writer = created;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_writer_authenticate(
    mtpl_tile_core_reader_t *reader) {
    size_t range_index;

    if (reader->mode != MTPL_STORAGE_ENCRYPTED) {
        return MTPL_STATUS_OK;
    }
    if (reader->key.data == NULL || reader->key.size == 0u) {
        return MTPL_STATUS_KEY_REQUIRED;
    }
    for (range_index = 0; range_index < reader->range_count; ++range_index) {
        const mtpl_tile_core_range_t *range = &reader->ranges[range_index];
        size_t entry_index;
        uint64_t width =
            (uint64_t)range->bounds.x_max - range->bounds.x_min + 1u;
        for (entry_index = 0; entry_index < range->entry_count; ++entry_index) {
            mtpl_buffer_t probe = {NULL, 0};
            mtpl_tile_coordinate_t coordinate;
            mtpl_status_t status;

            if (range->entries[entry_index] == 0) {
                continue;
            }
            coordinate.zoom = range->bounds.zoom;
            coordinate.x = range->bounds.x_min +
                (uint32_t)((uint64_t)entry_index % width);
            coordinate.y = range->bounds.y_min +
                (uint32_t)((uint64_t)entry_index / width);
            status = mtpl_tile_core_reader_read_tile(
                reader,
                &coordinate,
                &probe);
            free(probe.data);
            return status;
        }
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_writer_adopt_reader(
    const char *path,
    mtpl_tile_core_reader_t *reader,
    mtpl_tile_core_writer_t **writer) {
    mtpl_tile_core_writer_t *opened;
    mtpl_status_t status;

    opened = (mtpl_tile_core_writer_t *)calloc(1, sizeof(*opened));
    if (opened == NULL) {
        (void)mtpl_tile_core_reader_close(reader);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(opened->magic, reader->magic, sizeof(opened->magic));
    opened->mode = reader->mode;
    opened->tile_size = reader->tile_size;
    opened->existing = true;
    opened->data_started = true;
    opened->range_count = reader->range_count;
    opened->range_capacity = reader->range_count;
    opened->ranges = reader->ranges;
    reader->ranges = NULL;
    reader->range_count = 0;
    opened->key = reader->key;
    reader->key.data = NULL;
    reader->key.size = 0;
    status = mtpl_tile_core_reader_close(reader);
    if (status != MTPL_STATUS_OK) {
        mtpl_tile_writer_destroy(opened);
        return status;
    }
    opened->file = mtpl_fopen_utf8(path, "r+b");
    if (opened->file == NULL) {
        status = mtpl_tile_file_open_status();
        mtpl_tile_writer_destroy(opened);
        return status;
    }
    *writer = opened;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_writer_open(
    const char *path,
    const char expected_magic[4],
    const mtpl_crypto_options_t *crypto,
    mtpl_tile_core_writer_t **writer) {
    mtpl_tile_core_reader_t *reader = NULL;
    mtpl_status_t status;

    if (writer != NULL) {
        *writer = NULL;
    }
    if (path == NULL || path[0] == '\0' || expected_magic == NULL || writer == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_reader_open(path, expected_magic, crypto, &reader);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_tile_writer_authenticate(reader);
    if (status != MTPL_STATUS_OK) {
        (void)mtpl_tile_core_reader_close(reader);
        return status;
    }
    return mtpl_tile_writer_adopt_reader(path, reader, writer);
}

mtpl_status_t mtpl_tile_core_writer_add_range(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_range_t *range) {
    mtpl_tile_core_range_t *new_ranges;
    mtpl_tile_core_range_t *added;
    size_t entry_count;
    size_t range_index;
    size_t new_capacity;
    uint64_t added_index_bytes;
    mtpl_status_t status;

    if (writer == NULL || range == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (writer->existing || writer->data_started) {
        return MTPL_STATUS_UNSUPPORTED;
    }
    status = mtpl_tile_range_entry_count(range, &entry_count);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (entry_count > (size_t)(MTPL_TILE_MAX_INDEX_BYTES / sizeof(uint64_t))) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    added_index_bytes = (uint64_t)entry_count * sizeof(uint64_t);
    if (writer->total_index_bytes >
        MTPL_TILE_MAX_INDEX_BYTES - added_index_bytes) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    for (range_index = 0; range_index < writer->range_count; ++range_index) {
        if (mtpl_tile_ranges_overlap(&writer->ranges[range_index].bounds, range)) {
            return MTPL_STATUS_ALREADY_EXISTS;
        }
    }
    if (writer->range_count >= MTPL_TILE_MAX_RANGE_COUNT) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    if (writer->range_count == writer->range_capacity) {
        new_capacity = writer->range_capacity == 0 ? 4u : writer->range_capacity * 2u;
        if (new_capacity < writer->range_capacity ||
            new_capacity > SIZE_MAX / sizeof(*writer->ranges)) {
            return MTPL_STATUS_LIMIT_EXCEEDED;
        }
        new_ranges = (mtpl_tile_core_range_t *)realloc(
            writer->ranges,
            new_capacity * sizeof(*writer->ranges));
        if (new_ranges == NULL) {
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        memset(
            new_ranges + writer->range_capacity,
            0,
            (new_capacity - writer->range_capacity) * sizeof(*writer->ranges));
        writer->ranges = new_ranges;
        writer->range_capacity = new_capacity;
    }
    added = &writer->ranges[writer->range_count];
    memset(added, 0, sizeof(*added));
    added->bounds = *range;
    added->entry_count = entry_count;
    added->entries = (uint64_t *)calloc(entry_count, sizeof(*added->entries));
    if (added->entries == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    ++writer->range_count;
    writer->total_index_bytes += added_index_bytes;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_find_writer_entry(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_tile_core_range_t **found_range,
    size_t *found_index) {
    size_t range_index;
    if (writer == NULL || coordinate == NULL ||
        found_range == NULL || found_index == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    for (range_index = 0; range_index < writer->range_count; ++range_index) {
        mtpl_tile_core_range_t *range = &writer->ranges[range_index];
        if (coordinate->zoom == range->bounds.zoom &&
            coordinate->x >= range->bounds.x_min &&
            coordinate->x <= range->bounds.x_max &&
            coordinate->y >= range->bounds.y_min &&
            coordinate->y <= range->bounds.y_max) {
            uint64_t width = (uint64_t)range->bounds.x_max - range->bounds.x_min + 1u;
            uint64_t entry_index =
                ((uint64_t)coordinate->y - range->bounds.y_min) * width +
                ((uint64_t)coordinate->x - range->bounds.x_min);
            if (entry_index >= range->entry_count) {
                return MTPL_STATUS_INTERNAL_ERROR;
            }
            *found_range = range;
            *found_index = (size_t)entry_index;
            return MTPL_STATUS_OK;
        }
    }
    return MTPL_STATUS_OUT_OF_RANGE;
}

mtpl_status_t mtpl_tile_core_writer_has_tile(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    bool *exists) {
    mtpl_tile_core_range_t *range;
    size_t entry_index;
    mtpl_status_t status;
    if (exists != NULL) {
        *exists = false;
    }
    if (exists == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_find_writer_entry(writer, coordinate, &range, &entry_index);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    *exists = range->entries[entry_index] != 0;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_writer_store_frame(
    mtpl_tile_core_writer_t *writer,
    mtpl_tile_core_range_t *range,
    size_t entry_index,
    mtpl_buffer_view_t stored) {
    uint64_t file_size;
    uint64_t packed;
    mtpl_status_t status;

    if (writer == NULL || range == NULL || stored.data == NULL ||
        stored.size == 0u) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (stored.size > MTPL_TILE_INDEX_SIZE_MAX) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    if (range->entries[entry_index] != 0) {
        return MTPL_STATUS_ALREADY_EXISTS;
    }
    if (!writer->data_started) {
        status = mtpl_tile_writer_write_header(writer);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        writer->data_started = true;
    }
    status = mtpl_file_get_size(writer->file, &file_size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (file_size > MTPL_TILE_OFFSET_MAX ||
        stored.size > UINT64_MAX - file_size) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    status = mtpl_file_seek(writer->file, file_size);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_write_exact(writer->file, stored.data, stored.size);
    }
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    packed = ((uint64_t)stored.size << 40u) | file_size;
    range->entries[entry_index] = packed;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_tile_core_writer_add_tile_raw(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_view_t tile) {
    mtpl_tile_core_range_t *range;
    size_t entry_index;
    mtpl_status_t status;

    if (writer == NULL || coordinate == NULL || tile.data == NULL ||
        tile.size == 0u) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_find_writer_entry(writer, coordinate, &range, &entry_index);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    return mtpl_tile_writer_store_frame(
        writer,
        range,
        entry_index,
        tile);
}

mtpl_status_t mtpl_tile_core_writer_add_tile_data(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_view_t tile) {
    mtpl_tile_core_range_t *range;
    size_t entry_index;
    mtpl_buffer_t encoded = {NULL, 0};
    mtpl_buffer_view_t stored = tile;
    mtpl_status_t status;

    if (writer == NULL || coordinate == NULL || tile.data == NULL || tile.size == 0) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (tile.size > UINT32_MAX || tile.size > MTPL_TILE_MAX_LOGICAL_SIZE) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    status = mtpl_tile_find_writer_entry(writer, coordinate, &range, &entry_index);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (range->entries[entry_index] != 0) {
        return MTPL_STATUS_ALREADY_EXISTS;
    }
    if (writer->mode == MTPL_STORAGE_ENCRYPTED) {
        status = mtpl_crypto_encrypt_frame(
            &writer->key,
            tile,
            MTPL_COMPRESSION_SNAPPY,
            &encoded);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        stored.data = encoded.data;
        stored.size = encoded.size;
    }
    status = mtpl_tile_writer_store_frame(
        writer,
        range,
        entry_index,
        stored);
    free(encoded.data);
    return status;
}

mtpl_status_t mtpl_tile_core_writer_add_tile_file(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    const char *path) {
    FILE *file;
    uint64_t file_size;
    mtpl_buffer_t data = {NULL, 0};
    mtpl_status_t status;

    if (writer == NULL || coordinate == NULL || path == NULL || path[0] == '\0') {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_path_is_valid_utf8(path)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    file = mtpl_fopen_utf8(path, "rb");
    if (file == NULL) {
        return mtpl_tile_file_open_status();
    }
    status = mtpl_file_get_size(file, &file_size);
    if (status != MTPL_STATUS_OK || file_size == 0 ||
        file_size > MTPL_TILE_MAX_LOGICAL_SIZE || file_size > SIZE_MAX) {
        fclose(file);
        return status != MTPL_STATUS_OK
            ? status
            : (file_size == 0 ? MTPL_STATUS_INVALID_ARGUMENT : MTPL_STATUS_LIMIT_EXCEEDED);
    }
    status = mtpl_file_seek(file, 0);
    if (status != MTPL_STATUS_OK) {
        fclose(file);
        return status;
    }
    data.data = (uint8_t *)malloc((size_t)file_size);
    if (data.data == NULL) {
        fclose(file);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    status = mtpl_tile_read_exact(file, data.data, (size_t)file_size);
    if (status != MTPL_STATUS_OK) {
        free(data.data);
        fclose(file);
        return status;
    }
    data.size = (size_t)file_size;
    if (fclose(file) != 0) {
        free(data.data);
        return MTPL_STATUS_IO_ERROR;
    }
    status = mtpl_tile_core_writer_add_tile_data(
        writer,
        coordinate,
        (mtpl_buffer_view_t){data.data, data.size});
    free(data.data);
    return status;
}

mtpl_status_t mtpl_tile_core_writer_close(mtpl_tile_core_writer_t *writer) {
    mtpl_status_t status;
    if (writer == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = writer->existing
        ? mtpl_tile_writer_write_existing_indices(writer)
        : mtpl_tile_writer_write_header(writer);
    if (writer->file != NULL && fclose(writer->file) != 0 &&
        status == MTPL_STATUS_OK) {
        status = MTPL_STATUS_IO_ERROR;
    }
    writer->file = NULL;
    mtpl_tile_writer_destroy(writer);
    return status;
}

static void mtpl_tile_coordinate_from_entry(
    const mtpl_tile_core_range_t *range,
    size_t entry_index,
    mtpl_tile_coordinate_t *coordinate) {
    uint64_t width =
        (uint64_t)range->bounds.x_max - range->bounds.x_min + 1u;
    coordinate->zoom = range->bounds.zoom;
    coordinate->x = range->bounds.x_min +
        (uint32_t)((uint64_t)entry_index % width);
    coordinate->y = range->bounds.y_min +
        (uint32_t)((uint64_t)entry_index / width);
}

static mtpl_status_t mtpl_tile_merge_validate(
    mtpl_tile_core_reader_t *source,
    mtpl_tile_core_reader_t *destination) {
    size_t range_index;

    if (memcmp(source->magic, destination->magic, MTPL_TILE_MAGIC_SIZE) != 0 ||
        source->tile_size != destination->tile_size ||
        source->mode != destination->mode) {
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
    for (range_index = 0; range_index < source->range_count; ++range_index) {
        const mtpl_tile_core_range_t *range = &source->ranges[range_index];
        size_t entry_index;
        for (entry_index = 0; entry_index < range->entry_count; ++entry_index) {
            mtpl_tile_coordinate_t coordinate;
            mtpl_tile_core_range_t *destination_range;
            size_t destination_index;
            mtpl_status_t status;

            if (range->entries[entry_index] == 0) {
                continue;
            }
            mtpl_tile_coordinate_from_entry(range, entry_index, &coordinate);
            status = mtpl_tile_find_reader_entry(
                destination,
                &coordinate,
                &destination_range,
                &destination_index);
            if (status != MTPL_STATUS_OK) {
                return status;
            }
            (void)destination_range;
            (void)destination_index;
        }
    }
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_tile_core_merge(
    const char *source_path,
    const char *destination_path,
    const char expected_magic[4],
    const mtpl_crypto_options_t *crypto) {
    mtpl_tile_core_reader_t *source = NULL;
    mtpl_tile_core_reader_t *destination = NULL;
    mtpl_tile_core_writer_t *writer = NULL;
    mtpl_status_t status = MTPL_STATUS_OK;
    mtpl_status_t close_status;
    size_t range_index;
    bool same_file = false;

    if (source_path == NULL || destination_path == NULL ||
        expected_magic == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_path_is_valid_utf8(source_path) ||
        !mtpl_path_is_valid_utf8(destination_path)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    (void)crypto;
    status = mtpl_tile_core_reader_open(
        source_path,
        expected_magic,
        NULL,
        &source);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_tile_core_reader_open(
        destination_path,
        expected_magic,
        NULL,
        &destination);
    if (status != MTPL_STATUS_OK) {
        goto cleanup;
    }
    status = mtpl_file_is_same(source->file, destination->file, &same_file);
    if (status != MTPL_STATUS_OK) {
        goto cleanup;
    }
    if (same_file) {
        status = MTPL_STATUS_INVALID_ARGUMENT;
        goto cleanup;
    }
    status = mtpl_tile_merge_validate(source, destination);
    if (status != MTPL_STATUS_OK) {
        goto cleanup;
    }
    status = mtpl_tile_writer_adopt_reader(
        destination_path,
        destination,
        &writer);
    destination = NULL;
    if (status != MTPL_STATUS_OK) {
        goto cleanup;
    }

    for (range_index = 0;
         range_index < source->range_count && status == MTPL_STATUS_OK;
         ++range_index) {
        const mtpl_tile_core_range_t *range = &source->ranges[range_index];
        size_t entry_index;
        for (entry_index = 0;
             entry_index < range->entry_count && status == MTPL_STATUS_OK;
             ++entry_index) {
            mtpl_tile_coordinate_t coordinate;
            mtpl_tile_core_range_t *destination_range;
            size_t destination_index;
            mtpl_buffer_t tile = {NULL, 0};

            if (range->entries[entry_index] == 0) {
                continue;
            }
            mtpl_tile_coordinate_from_entry(range, entry_index, &coordinate);
            status = mtpl_tile_find_writer_entry(
                writer,
                &coordinate,
                &destination_range,
                &destination_index);
            if (status != MTPL_STATUS_OK) {
                break;
            }
            if (destination_range->entries[destination_index] != 0) {
                continue;
            }
            status = mtpl_tile_core_reader_read_tile_raw(
                source,
                &coordinate,
                &tile);
            if (status == MTPL_STATUS_OK) {
                status = mtpl_tile_core_writer_add_tile_raw(
                    writer,
                    &coordinate,
                    (mtpl_buffer_view_t){tile.data, tile.size});
            }
            free(tile.data);
        }
    }

cleanup:
    if (source != NULL) {
        close_status = mtpl_tile_core_reader_close(source);
        if (status == MTPL_STATUS_OK && close_status != MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (destination != NULL) {
        close_status = mtpl_tile_core_reader_close(destination);
        if (status == MTPL_STATUS_OK && close_status != MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (writer != NULL) {
        close_status = mtpl_tile_core_writer_close(writer);
        if (status == MTPL_STATUS_OK && close_status != MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    return status;
}
