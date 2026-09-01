#ifndef MTPL_TILE_CORE_H
#define MTPL_TILE_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include <mtpl/status.h>
#include <mtpl/types.h>

typedef struct mtpl_tile_core_reader mtpl_tile_core_reader_t;
typedef struct mtpl_tile_core_writer mtpl_tile_core_writer_t;

mtpl_status_t mtpl_tile_core_reader_open(
    const char *path,
    const char expected_magic[4],
    const mtpl_crypto_options_t *crypto,
    mtpl_tile_core_reader_t **reader);
mtpl_status_t mtpl_tile_core_reader_read_tile(
    mtpl_tile_core_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile);
mtpl_status_t mtpl_tile_core_reader_read_tile_raw(
    mtpl_tile_core_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile);
mtpl_status_t mtpl_tile_core_reader_read_metadata(
    mtpl_tile_core_reader_t *reader,
    mtpl_buffer_t *metadata);
mtpl_status_t mtpl_tile_core_reader_check_edge(
    mtpl_tile_core_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    uint8_t *edge);
mtpl_status_t mtpl_tile_core_reader_get_tile_size(
    mtpl_tile_core_reader_t *reader,
    uint32_t *tile_size);
mtpl_status_t mtpl_tile_core_reader_get_storage_mode(
    mtpl_tile_core_reader_t *reader,
    mtpl_storage_mode_t *mode);
mtpl_status_t mtpl_tile_core_reader_get_range_count(
    const mtpl_tile_core_reader_t *reader,
    size_t *range_count);
mtpl_status_t mtpl_tile_core_reader_get_range_info(
    const mtpl_tile_core_reader_t *reader,
    size_t range_index,
    mtpl_tile_range_info_t *info);
mtpl_status_t mtpl_tile_core_reader_get_entry_info(
    const mtpl_tile_core_reader_t *reader,
    size_t range_index,
    size_t slot_index,
    mtpl_tile_entry_info_t *info);
mtpl_status_t mtpl_tile_core_reader_close(mtpl_tile_core_reader_t *reader);

mtpl_status_t mtpl_tile_core_writer_create(
    const char *path,
    const char magic[4],
    uint32_t tile_size,
    mtpl_buffer_view_t metadata,
    mtpl_storage_mode_t mode,
    const mtpl_crypto_options_t *crypto,
    mtpl_tile_core_writer_t **writer);
mtpl_status_t mtpl_tile_core_writer_open(
    const char *path,
    const char expected_magic[4],
    const mtpl_crypto_options_t *crypto,
    mtpl_tile_core_writer_t **writer);
mtpl_status_t mtpl_tile_core_writer_add_range(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_range_t *range);
mtpl_status_t mtpl_tile_core_writer_add_tile_file(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    const char *path);
mtpl_status_t mtpl_tile_core_writer_add_tile_data(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_view_t tile);
mtpl_status_t mtpl_tile_core_writer_has_tile(
    mtpl_tile_core_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    bool *exists);
mtpl_status_t mtpl_tile_core_writer_close(mtpl_tile_core_writer_t *writer);

mtpl_status_t mtpl_tile_core_merge(
    const char *source_path,
    const char *destination_path,
    const char expected_magic[4],
    const mtpl_crypto_options_t *crypto);

#endif
