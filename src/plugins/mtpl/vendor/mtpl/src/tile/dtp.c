#include <mtpl/dtp.h>

#include <stdlib.h>
#include <string.h>

#include "tile_core.h"

static const char mtpl_dtp_magic[4] = {'D', 'T', 'P', '\0'};

mtpl_status_t mtpl_dtp_reader_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_dtp_reader_t **reader) {
    mtpl_tile_core_reader_t *core = NULL;
    mtpl_status_t status;
    if (reader != NULL) {
        *reader = NULL;
    }
    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_reader_open(path, mtpl_dtp_magic, crypto, &core);
    if (status == MTPL_STATUS_OK) {
        *reader = (mtpl_dtp_reader_t *)core;
    }
    return status;
}

mtpl_status_t mtpl_dtp_reader_read_tile(
    mtpl_dtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile) {
    return mtpl_tile_core_reader_read_tile(
        (mtpl_tile_core_reader_t *)reader, coordinate, tile);
}

mtpl_status_t mtpl_dtp_reader_read_tile_raw(
    mtpl_dtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile) {
    return mtpl_tile_core_reader_read_tile_raw(
        (mtpl_tile_core_reader_t *)reader, coordinate, tile);
}

mtpl_status_t mtpl_dtp_reader_read_tile_cropped(
    mtpl_dtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_t *tile) {
    mtpl_buffer_t source = {NULL, 0};
    uint32_t source_width;
    uint32_t destination_width;
    size_t source_row_size;
    size_t destination_row_size;
    size_t expected_size;
    uint32_t row;
    mtpl_status_t status;

    if (tile != NULL) {
        tile->data = NULL;
        tile->size = 0;
    }
    if (reader == NULL || coordinate == NULL || tile == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_reader_get_tile_size(
        (mtpl_tile_core_reader_t *)reader,
        &source_width);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (source_width == 33u) {
        destination_width = 32u;
    } else if (source_width == 129u) {
        destination_width = 128u;
    } else {
        return MTPL_STATUS_UNSUPPORTED;
    }
    expected_size = (size_t)source_width * source_width * 2u;
    status = mtpl_tile_core_reader_read_tile(
        (mtpl_tile_core_reader_t *)reader,
        coordinate,
        &source);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (source.size != expected_size) {
        free(source.data);
        return MTPL_STATUS_CORRUPT_DATA;
    }
    destination_row_size = (size_t)destination_width * 2u;
    source_row_size = (size_t)source_width * 2u;
    tile->size = destination_row_size * destination_width;
    tile->data = (uint8_t *)malloc(tile->size);
    if (tile->data == NULL) {
        tile->size = 0;
        free(source.data);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    for (row = 0; row < destination_width; ++row) {
        memcpy(
            tile->data + (size_t)row * destination_row_size,
            source.data + (size_t)row * source_row_size,
            destination_row_size);
    }
    free(source.data);
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_dtp_reader_read_metadata(
    mtpl_dtp_reader_t *reader,
    mtpl_buffer_t *metadata) {
    return mtpl_tile_core_reader_read_metadata(
        (mtpl_tile_core_reader_t *)reader, metadata);
}

mtpl_status_t mtpl_dtp_reader_check_edge(
    mtpl_dtp_reader_t *reader,
    const mtpl_tile_coordinate_t *coordinate,
    uint8_t *edge) {
    return mtpl_tile_core_reader_check_edge(
        (mtpl_tile_core_reader_t *)reader, coordinate, edge);
}

mtpl_status_t mtpl_dtp_reader_get_tile_size(
    mtpl_dtp_reader_t *reader,
    uint32_t *tile_size) {
    return mtpl_tile_core_reader_get_tile_size(
        (mtpl_tile_core_reader_t *)reader, tile_size);
}

mtpl_status_t mtpl_dtp_reader_get_storage_mode(
    mtpl_dtp_reader_t *reader,
    mtpl_storage_mode_t *mode) {
    return mtpl_tile_core_reader_get_storage_mode(
        (mtpl_tile_core_reader_t *)reader, mode);
}

mtpl_status_t mtpl_dtp_reader_get_range_count(
    const mtpl_dtp_reader_t *reader,
    size_t *range_count) {
    return mtpl_tile_core_reader_get_range_count(
        (const mtpl_tile_core_reader_t *)reader, range_count);
}

mtpl_status_t mtpl_dtp_reader_get_range_info(
    const mtpl_dtp_reader_t *reader,
    size_t range_index,
    mtpl_tile_range_info_t *info) {
    return mtpl_tile_core_reader_get_range_info(
        (const mtpl_tile_core_reader_t *)reader, range_index, info);
}

mtpl_status_t mtpl_dtp_reader_get_entry_info(
    const mtpl_dtp_reader_t *reader,
    size_t range_index,
    size_t slot_index,
    mtpl_tile_entry_info_t *info) {
    return mtpl_tile_core_reader_get_entry_info(
        (const mtpl_tile_core_reader_t *)reader,
        range_index,
        slot_index,
        info);
}

mtpl_status_t mtpl_dtp_reader_close(mtpl_dtp_reader_t *reader) {
    return mtpl_tile_core_reader_close((mtpl_tile_core_reader_t *)reader);
}

mtpl_status_t mtpl_dtp_writer_create(
    const char *path,
    uint32_t tile_size,
    mtpl_buffer_view_t metadata,
    mtpl_storage_mode_t mode,
    const mtpl_crypto_options_t *crypto,
    mtpl_dtp_writer_t **writer) {
    mtpl_tile_core_writer_t *core = NULL;
    mtpl_status_t status;
    if (writer != NULL) {
        *writer = NULL;
    }
    if (writer == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_writer_create(
        path, mtpl_dtp_magic, tile_size, metadata, mode, crypto, &core);
    if (status == MTPL_STATUS_OK) {
        *writer = (mtpl_dtp_writer_t *)core;
    }
    return status;
}

mtpl_status_t mtpl_dtp_writer_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_dtp_writer_t **writer) {
    mtpl_tile_core_writer_t *core = NULL;
    mtpl_status_t status;
    if (writer != NULL) {
        *writer = NULL;
    }
    if (writer == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_tile_core_writer_open(path, mtpl_dtp_magic, crypto, &core);
    if (status == MTPL_STATUS_OK) {
        *writer = (mtpl_dtp_writer_t *)core;
    }
    return status;
}

mtpl_status_t mtpl_dtp_writer_add_range(
    mtpl_dtp_writer_t *writer,
    const mtpl_tile_range_t *range) {
    return mtpl_tile_core_writer_add_range(
        (mtpl_tile_core_writer_t *)writer, range);
}

mtpl_status_t mtpl_dtp_writer_add_tile_file(
    mtpl_dtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    const char *path) {
    return mtpl_tile_core_writer_add_tile_file(
        (mtpl_tile_core_writer_t *)writer, coordinate, path);
}

mtpl_status_t mtpl_dtp_writer_add_tile_data(
    mtpl_dtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    mtpl_buffer_view_t tile) {
    return mtpl_tile_core_writer_add_tile_data(
        (mtpl_tile_core_writer_t *)writer, coordinate, tile);
}

mtpl_status_t mtpl_dtp_writer_has_tile(
    mtpl_dtp_writer_t *writer,
    const mtpl_tile_coordinate_t *coordinate,
    bool *exists) {
    return mtpl_tile_core_writer_has_tile(
        (mtpl_tile_core_writer_t *)writer, coordinate, exists);
}

mtpl_status_t mtpl_dtp_writer_close(mtpl_dtp_writer_t *writer) {
    return mtpl_tile_core_writer_close((mtpl_tile_core_writer_t *)writer);
}

mtpl_status_t mtpl_dtp_merge(
    const char *source_path,
    const char *destination_path,
    const mtpl_crypto_options_t *crypto) {
    return mtpl_tile_core_merge(
        source_path, destination_path, mtpl_dtp_magic, crypto);
}
