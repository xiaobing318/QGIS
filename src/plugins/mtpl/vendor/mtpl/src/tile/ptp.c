#include <mtpl/ptp.h>

#include "tile_core.h"

static const char mtpl_ptp_magic[4] = {'P', 'T', 'P', '\0'};

mtpl_status_t mtpl_ptp_reader_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_ptp_reader_t **reader) {
    mtpl_tile_core_reader_t *core = NULL;
    mtpl_status_t status;
    if (reader != NULL) {
        *reader = NULL;
    }
    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_reader_open(path, mtpl_ptp_magic, crypto, &core);
    if (status == MTPL_STATUS_OK) {
        *reader = (mtpl_ptp_reader_t *)core;
    }
    return status;
}

mtpl_status_t mtpl_ptp_reader_read_tile(
    mtpl_ptp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile) {
    return mtpl_tile_core_reader_read_tile(
        (mtpl_tile_core_reader_t *)reader, coordinate, tile);
}

mtpl_status_t mtpl_ptp_reader_read_tile_raw(
    mtpl_ptp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile) {
    return mtpl_tile_core_reader_read_tile_raw(
        (mtpl_tile_core_reader_t *)reader, coordinate, tile);
}

mtpl_status_t mtpl_ptp_reader_read_metadata(
    mtpl_ptp_reader_t *reader,
    mtpl_buffer_t *metadata) {
    return mtpl_tile_core_reader_read_metadata(
        (mtpl_tile_core_reader_t *)reader, metadata);
}

mtpl_status_t mtpl_ptp_reader_check_edge(
    mtpl_ptp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    uint8_t *edge) {
    return mtpl_tile_core_reader_check_edge(
        (mtpl_tile_core_reader_t *)reader, coordinate, edge);
}

mtpl_status_t mtpl_ptp_reader_get_tile_size(
    mtpl_ptp_reader_t *reader,
    uint32_t *tile_size) {
    return mtpl_tile_core_reader_get_tile_size(
        (mtpl_tile_core_reader_t *)reader, tile_size);
}

mtpl_status_t mtpl_ptp_reader_get_storage_mode(
    mtpl_ptp_reader_t *reader,
    mtpl_storage_mode_t *mode) {
    return mtpl_tile_core_reader_get_storage_mode(
        (mtpl_tile_core_reader_t *)reader, mode);
}

mtpl_status_t mtpl_ptp_reader_get_range_count(
    const mtpl_ptp_reader_t *reader,
    size_t *range_count) {
    return mtpl_tile_core_reader_get_range_count(
        (const mtpl_tile_core_reader_t *)reader, range_count);
}

mtpl_status_t mtpl_ptp_reader_get_range_info(
    const mtpl_ptp_reader_t *reader,
    size_t range_index,
    mtpl_tile_range_info_t *info) {
    return mtpl_tile_core_reader_get_range_info(
        (const mtpl_tile_core_reader_t *)reader, range_index, info);
}

mtpl_status_t mtpl_ptp_reader_get_entry_info(
    const mtpl_ptp_reader_t *reader,
    size_t range_index,
    size_t slot_index,
    mtpl_tile_entry_info_t *info) {
    return mtpl_tile_core_reader_get_entry_info(
        (const mtpl_tile_core_reader_t *)reader,
        range_index,
        slot_index,
        info);
}

mtpl_status_t mtpl_ptp_reader_close(mtpl_ptp_reader_t *reader) {
    return mtpl_tile_core_reader_close((mtpl_tile_core_reader_t *)reader);
}

mtpl_status_t mtpl_ptp_writer_create(
    const char *path,
    uint32_t tile_size,
    mtpl_buffer_view_t metadata,
    mtpl_storage_mode_t mode,
    const mtpl_crypto_options_t *crypto,
    mtpl_ptp_writer_t **writer) {
    mtpl_tile_core_writer_t *core = NULL;
    mtpl_status_t status;
    if (writer != NULL) {
        *writer = NULL;
    }
    if (writer == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_writer_create(
        path, mtpl_ptp_magic, tile_size, metadata, mode, crypto, &core);
    if (status == MTPL_STATUS_OK) {
        *writer = (mtpl_ptp_writer_t *)core;
    }
    return status;
}

mtpl_status_t mtpl_ptp_writer_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_ptp_writer_t **writer) {
    mtpl_tile_core_writer_t *core = NULL;
    mtpl_status_t status;
    if (writer != NULL) {
        *writer = NULL;
    }
    if (writer == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_writer_open(path, mtpl_ptp_magic, crypto, &core);
    if (status == MTPL_STATUS_OK) {
        *writer = (mtpl_ptp_writer_t *)core;
    }
    return status;
}

mtpl_status_t mtpl_ptp_writer_add_range(
    mtpl_ptp_writer_t *writer,
    const mtpl_tile_range_t *range) {
    return mtpl_tile_core_writer_add_range(
        (mtpl_tile_core_writer_t *)writer, range);
}

mtpl_status_t mtpl_ptp_writer_add_tile_file(
    mtpl_ptp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    const char *path) {
    return mtpl_tile_core_writer_add_tile_file(
        (mtpl_tile_core_writer_t *)writer, coordinate, path);
}

mtpl_status_t mtpl_ptp_writer_add_tile_data(
    mtpl_ptp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_view_t tile) {
    return mtpl_tile_core_writer_add_tile_data(
        (mtpl_tile_core_writer_t *)writer, coordinate, tile);
}

mtpl_status_t mtpl_ptp_writer_has_tile(
    mtpl_ptp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    bool *exists) {
    return mtpl_tile_core_writer_has_tile(
        (mtpl_tile_core_writer_t *)writer, coordinate, exists);
}

mtpl_status_t mtpl_ptp_writer_close(mtpl_ptp_writer_t *writer) {
    return mtpl_tile_core_writer_close((mtpl_tile_core_writer_t *)writer);
}

mtpl_status_t mtpl_ptp_merge(
    const char *source_path,
    const char *destination_path,
    const mtpl_crypto_options_t *crypto) {
    return mtpl_tile_core_merge(
        source_path, destination_path, mtpl_ptp_magic, crypto);
}
