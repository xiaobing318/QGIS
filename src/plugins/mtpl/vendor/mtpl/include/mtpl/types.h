#ifndef MTPL_TYPES_H
#define MTPL_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mtpl_storage_mode {
    MTPL_STORAGE_PLAIN = 0,
    MTPL_STORAGE_ENCRYPTED = 1
} mtpl_storage_mode_t;

typedef struct mtpl_crypto_options {
    const uint8_t *private_key;
    size_t private_key_size;
    const uint8_t *device_key;
    size_t device_key_size;
} mtpl_crypto_options_t;

typedef struct mtpl_buffer_view {
    const uint8_t *data;
    size_t size;
} mtpl_buffer_view_t;

typedef struct mtpl_buffer {
    uint8_t *data;
    size_t size;
} mtpl_buffer_t;

typedef struct mtpl_tile_coordinate {
    uint32_t zoom;
    uint32_t x;
    uint32_t y;
} mtpl_tile_coordinate_t;

typedef struct mtpl_tile_range {
    uint32_t zoom;
    uint32_t x_min;
    uint32_t x_max;
    uint32_t y_min;
    uint32_t y_max;
} mtpl_tile_range_t;

typedef struct mtpl_tile_range_info {
    mtpl_tile_range_t bounds;
    size_t slot_count;
    size_t present_count;
} mtpl_tile_range_info_t;

typedef struct mtpl_tile_entry_info {
    mtpl_tile_coordinate_t coordinate;
    bool present;
    uint64_t stored_size;
} mtpl_tile_entry_info_t;

#ifdef __cplusplus
}
#endif

#endif
