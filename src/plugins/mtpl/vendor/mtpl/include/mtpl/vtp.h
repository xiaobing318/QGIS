#ifndef MTPL_VTP_H
#define MTPL_VTP_H

#include <stdbool.h>
#include <stdint.h>

#include <mtpl/export.h>
#include <mtpl/status.h>
#include <mtpl/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mtpl_vtp_reader mtpl_vtp_reader_t;
typedef struct mtpl_vtp_writer mtpl_vtp_writer_t;

MTPL_API mtpl_status_t mtpl_vtp_reader_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_vtp_reader_t **reader);
MTPL_API mtpl_status_t mtpl_vtp_reader_read_tile(
    mtpl_vtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile);
MTPL_API mtpl_status_t mtpl_vtp_reader_read_tile_raw(
    mtpl_vtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile);
MTPL_API mtpl_status_t mtpl_vtp_reader_read_metadata(
    mtpl_vtp_reader_t *reader,
    mtpl_buffer_t *metadata);
MTPL_API mtpl_status_t mtpl_vtp_reader_check_edge(
    mtpl_vtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    uint8_t *edge);
MTPL_API mtpl_status_t mtpl_vtp_reader_get_tile_size(
    mtpl_vtp_reader_t *reader,
    uint32_t *tile_size);
MTPL_API mtpl_status_t mtpl_vtp_reader_get_storage_mode(
    mtpl_vtp_reader_t *reader,
    mtpl_storage_mode_t *mode);
MTPL_API mtpl_status_t mtpl_vtp_reader_get_range_count(
    const mtpl_vtp_reader_t *reader,
    size_t *range_count);
MTPL_API mtpl_status_t mtpl_vtp_reader_get_range_info(
    const mtpl_vtp_reader_t *reader,
    size_t range_index,
    mtpl_tile_range_info_t *info);
MTPL_API mtpl_status_t mtpl_vtp_reader_get_entry_info(
    const mtpl_vtp_reader_t *reader,
    size_t range_index,
    size_t slot_index,
    mtpl_tile_entry_info_t *info);
MTPL_API mtpl_status_t mtpl_vtp_reader_close(mtpl_vtp_reader_t *reader);

MTPL_API mtpl_status_t mtpl_vtp_writer_create(
    const char *path,
    uint32_t tile_size,
    mtpl_buffer_view_t metadata,
    mtpl_storage_mode_t mode,
    const mtpl_crypto_options_t *crypto,
    mtpl_vtp_writer_t **writer);
MTPL_API mtpl_status_t mtpl_vtp_writer_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_vtp_writer_t **writer);
MTPL_API mtpl_status_t mtpl_vtp_writer_add_range(
    mtpl_vtp_writer_t *writer,
    const mtpl_tile_range_t *range);
MTPL_API mtpl_status_t mtpl_vtp_writer_add_tile_file(
    mtpl_vtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    const char *path);
MTPL_API mtpl_status_t mtpl_vtp_writer_add_tile_data(
    mtpl_vtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_view_t tile);
MTPL_API mtpl_status_t mtpl_vtp_writer_has_tile(
    mtpl_vtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    bool *exists);
MTPL_API mtpl_status_t mtpl_vtp_writer_close(mtpl_vtp_writer_t *writer);

MTPL_API mtpl_status_t mtpl_vtp_merge(
    const char *source_path,
    const char *destination_path,
    const mtpl_crypto_options_t *crypto);

#ifdef __cplusplus
}
#endif

#endif
