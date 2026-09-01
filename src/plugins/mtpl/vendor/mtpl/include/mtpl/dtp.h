#ifndef MTPL_DTP_H
#define MTPL_DTP_H

#include <stdbool.h>
#include <stdint.h>

#include <mtpl/export.h>
#include <mtpl/status.h>
#include <mtpl/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mtpl_dtp_reader mtpl_dtp_reader_t;
typedef struct mtpl_dtp_writer mtpl_dtp_writer_t;

MTPL_API mtpl_status_t mtpl_dtp_reader_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_dtp_reader_t **reader);
MTPL_API mtpl_status_t mtpl_dtp_reader_read_tile(
    mtpl_dtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile);
MTPL_API mtpl_status_t mtpl_dtp_reader_read_tile_raw(
    mtpl_dtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile);
MTPL_API mtpl_status_t mtpl_dtp_reader_read_tile_cropped(
    mtpl_dtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile);
MTPL_API mtpl_status_t mtpl_dtp_reader_read_metadata(
    mtpl_dtp_reader_t *reader,
    mtpl_buffer_t *metadata);
MTPL_API mtpl_status_t mtpl_dtp_reader_check_edge(
    mtpl_dtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    uint8_t *edge);
MTPL_API mtpl_status_t mtpl_dtp_reader_get_tile_size(
    mtpl_dtp_reader_t *reader,
    uint32_t *tile_size);
MTPL_API mtpl_status_t mtpl_dtp_reader_get_storage_mode(
    mtpl_dtp_reader_t *reader,
    mtpl_storage_mode_t *mode);
MTPL_API mtpl_status_t mtpl_dtp_reader_get_range_count(
    const mtpl_dtp_reader_t *reader,
    size_t *range_count);
MTPL_API mtpl_status_t mtpl_dtp_reader_get_range_info(
    const mtpl_dtp_reader_t *reader,
    size_t range_index,
    mtpl_tile_range_info_t *info);
MTPL_API mtpl_status_t mtpl_dtp_reader_get_entry_info(
    const mtpl_dtp_reader_t *reader,
    size_t range_index,
    size_t slot_index,
    mtpl_tile_entry_info_t *info);
MTPL_API mtpl_status_t mtpl_dtp_reader_close(mtpl_dtp_reader_t *reader);

MTPL_API mtpl_status_t mtpl_dtp_writer_create(
    const char *path,
    uint32_t tile_size,
    mtpl_buffer_view_t metadata,
    mtpl_storage_mode_t mode,
    const mtpl_crypto_options_t *crypto,
    mtpl_dtp_writer_t **writer);
MTPL_API mtpl_status_t mtpl_dtp_writer_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_dtp_writer_t **writer);
MTPL_API mtpl_status_t mtpl_dtp_writer_add_range(
    mtpl_dtp_writer_t *writer,
    const mtpl_tile_range_t *range);
MTPL_API mtpl_status_t mtpl_dtp_writer_add_tile_file(
    mtpl_dtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    const char *path);
MTPL_API mtpl_status_t mtpl_dtp_writer_add_tile_data(
    mtpl_dtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_view_t tile);
MTPL_API mtpl_status_t mtpl_dtp_writer_has_tile(
    mtpl_dtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    bool *exists);
MTPL_API mtpl_status_t mtpl_dtp_writer_close(mtpl_dtp_writer_t *writer);

MTPL_API mtpl_status_t mtpl_dtp_merge(
    const char *source_path,
    const char *destination_path,
    const mtpl_crypto_options_t *crypto);

#ifdef __cplusplus
}
#endif

#endif
