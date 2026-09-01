#include <mtpl/package.h>
#include <mtpl/mtpl.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mtpl/sfp.h>

#include "../internal/crypto.h"
#include "../internal/file_io.h"
#include "../sfp/sfp_internal.h"
#include "../tile/tile_core.h"

#define MTPL_PACKAGE_MAGIC_SIZE 4u
#define MTPL_PACKAGE_TEMP_ATTEMPTS 1024u

static const char mtpl_package_ptp_magic[MTPL_PACKAGE_MAGIC_SIZE] = {
    'P', 'T', 'P', '\0'
};
static const char mtpl_package_dtp_magic[MTPL_PACKAGE_MAGIC_SIZE] = {
    'D', 'T', 'P', '\0'
};
static const char mtpl_package_vtp_magic[MTPL_PACKAGE_MAGIC_SIZE] = {
    'V', 'T', 'P', '\0'
};
static const char mtpl_package_sfp_magic[MTPL_PACKAGE_MAGIC_SIZE] = {
    'S', 'F', 'P', '\0'
};

static bool mtpl_package_info_is_compatible(const mtpl_package_info_t *info) {
    return info != NULL && info->struct_size >= sizeof(*info);
}

static void mtpl_package_zero_info(mtpl_package_info_t *info) {
    if (mtpl_package_info_is_compatible(info)) {
        uint32_t struct_size = info->struct_size;
        memset(info, 0, sizeof(*info));
        info->struct_size = struct_size;
    }
}

static mtpl_status_t mtpl_package_file_open_status(void) {
    return errno == ENOENT
        ? MTPL_STATUS_FILE_NOT_FOUND
        : MTPL_STATUS_IO_ERROR;
}

static mtpl_status_t mtpl_package_read_magic(
    const char *path,
    char magic[MTPL_PACKAGE_MAGIC_SIZE],
    uint64_t *file_size) {
    FILE *file;
    mtpl_status_t status;

    if (magic != NULL) {
        memset(magic, 0, MTPL_PACKAGE_MAGIC_SIZE);
    }
    if (file_size != NULL) {
        *file_size = 0u;
    }
    if (!mtpl_path_is_valid_utf8(path) || magic == NULL || file_size == NULL) {
        return path != NULL && path[0] != '\0'
            ? MTPL_STATUS_PATH_ERROR
            : MTPL_STATUS_INVALID_ARGUMENT;
    }
    file = mtpl_fopen_utf8(path, "rb");
    if (file == NULL) {
        return mtpl_package_file_open_status();
    }
    status = mtpl_file_get_size(file, file_size);
    if (status == MTPL_STATUS_OK && *file_size < MTPL_PACKAGE_MAGIC_SIZE) {
        status = MTPL_STATUS_FORMAT_ERROR;
    }
    if (status == MTPL_STATUS_OK &&
        fread(magic, 1u, MTPL_PACKAGE_MAGIC_SIZE, file) !=
            MTPL_PACKAGE_MAGIC_SIZE) {
        status = ferror(file) != 0
            ? MTPL_STATUS_IO_ERROR
            : MTPL_STATUS_FORMAT_ERROR;
    }
    if (fclose(file) != 0 && status == MTPL_STATUS_OK) {
        status = MTPL_STATUS_IO_ERROR;
    }
    return status;
}

static mtpl_package_storage_t mtpl_package_storage_from_mode(
    mtpl_storage_mode_t mode) {
    return mode == MTPL_STORAGE_ENCRYPTED
        ? MTPL_PACKAGE_STORAGE_ENCRYPTED
        : MTPL_PACKAGE_STORAGE_PLAIN;
}

static mtpl_status_t mtpl_package_probe_tile(
    const char *path,
    const char magic[MTPL_PACKAGE_MAGIC_SIZE],
    mtpl_package_format_t format,
    uint64_t file_size,
    mtpl_package_info_t *info) {
    mtpl_tile_core_reader_t *reader = NULL;
    mtpl_storage_mode_t mode = MTPL_STORAGE_PLAIN;
    size_t range_count = 0u;
    size_t range_index;
    mtpl_status_t status;

    status = mtpl_tile_core_reader_open(path, magic, NULL, &reader);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_core_reader_get_storage_mode(reader, &mode);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_core_reader_get_tile_size(reader, &info->tile_size);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_core_reader_get_range_count(reader, &range_count);
    }
    for (range_index = 0u;
         status == MTPL_STATUS_OK && range_index < range_count;
         ++range_index) {
        mtpl_tile_range_info_t range_info;
        status = mtpl_tile_core_reader_get_range_info(
            reader,
            range_index,
            &range_info);
        if (status == MTPL_STATUS_OK &&
            range_info.present_count > UINT64_MAX - info->entry_count) {
            status = MTPL_STATUS_LIMIT_EXCEEDED;
        }
        if (status == MTPL_STATUS_OK) {
            info->entry_count += range_info.present_count;
        }
    }
    if (reader != NULL) {
        mtpl_status_t close_status = mtpl_tile_core_reader_close(reader);
        if (status == MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (status != MTPL_STATUS_OK) {
        mtpl_package_zero_info(info);
        return status;
    }
    info->format = format;
    info->storage = mtpl_package_storage_from_mode(mode);
    info->format_version = 1u;
    info->file_size = file_size;
    info->range_count = (uint64_t)range_count;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_package_probe_sfp(
    const char *path,
    uint64_t file_size,
    mtpl_package_info_t *info) {
    mtpl_sfp_reader_t *reader = NULL;
    bool has_plain = false;
    bool has_encrypted = false;
    size_t entry_count = 0u;
    size_t entry_index;
    mtpl_status_t status = mtpl_sfp_reader_open(path, NULL, &reader);

    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_reader_get_entry_count(reader, &entry_count);
    }
    for (entry_index = 0u;
         status == MTPL_STATUS_OK && entry_index < entry_count;
         ++entry_index) {
        mtpl_sfp_entry_info_t entry_info = {
            NULL, 0u, 0u, MTPL_STORAGE_PLAIN
        };
        status = mtpl_sfp_reader_get_entry_info(
            reader,
            entry_index,
            &entry_info);
        if (status == MTPL_STATUS_OK &&
            entry_info.storage_mode == MTPL_STORAGE_ENCRYPTED) {
            has_encrypted = true;
        } else if (status == MTPL_STATUS_OK) {
            has_plain = true;
        }
    }
    if (reader != NULL) {
        mtpl_status_t close_status = mtpl_sfp_reader_close(reader);
        if (status == MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (status != MTPL_STATUS_OK) {
        mtpl_package_zero_info(info);
        return status;
    }
    info->format = MTPL_PACKAGE_FORMAT_SFP;
    info->storage = has_encrypted
        ? (has_plain
            ? MTPL_PACKAGE_STORAGE_MIXED
            : MTPL_PACKAGE_STORAGE_ENCRYPTED)
        : MTPL_PACKAGE_STORAGE_PLAIN;
    info->format_version = 2u;
    info->file_size = file_size;
    info->entry_count = (uint64_t)entry_count;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_package_probe(
    const char *path,
    mtpl_package_info_t *info) {
    char magic[MTPL_PACKAGE_MAGIC_SIZE];
    uint64_t file_size = 0u;
    mtpl_status_t status;

    if (info == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_package_info_is_compatible(info)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    mtpl_package_zero_info(info);
    if (path == NULL || path[0] == '\0') {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_package_read_magic(path, magic, &file_size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (memcmp(magic, mtpl_package_ptp_magic, sizeof(magic)) == 0) {
        return mtpl_package_probe_tile(
            path,
            mtpl_package_ptp_magic,
            MTPL_PACKAGE_FORMAT_PTP,
            file_size,
            info);
    }
    if (memcmp(magic, mtpl_package_dtp_magic, sizeof(magic)) == 0) {
        return mtpl_package_probe_tile(
            path,
            mtpl_package_dtp_magic,
            MTPL_PACKAGE_FORMAT_DTP,
            file_size,
            info);
    }
    if (memcmp(magic, mtpl_package_vtp_magic, sizeof(magic)) == 0) {
        return mtpl_package_probe_tile(
            path,
            mtpl_package_vtp_magic,
            MTPL_PACKAGE_FORMAT_VTP,
            file_size,
            info);
    }
    if (memcmp(magic, mtpl_package_sfp_magic, sizeof(magic)) == 0) {
        return mtpl_package_probe_sfp(path, file_size, info);
    }
    return MTPL_STATUS_FORMAT_MISMATCH;
}

static bool mtpl_package_is_validation_failure(mtpl_status_t status) {
    return status == MTPL_STATUS_CRYPTO_ERROR ||
        status == MTPL_STATUS_FORMAT_ERROR ||
        status == MTPL_STATUS_CORRUPT_DATA ||
        status == MTPL_STATUS_COMPRESSION_ERROR ||
        status == MTPL_STATUS_DECOMPRESSION_ERROR ||
        status == MTPL_STATUS_UNSUPPORTED_VERSION ||
        status == MTPL_STATUS_UNSUPPORTED ||
        status == MTPL_STATUS_LIMIT_EXCEEDED;
}

static mtpl_status_t mtpl_package_validation_result(
    mtpl_status_t operation_status,
    mtpl_key_validation_t *validation) {
    if (operation_status == MTPL_STATUS_OK) {
        *validation = MTPL_KEY_VALIDATION_VALID;
        return MTPL_STATUS_OK;
    }
    if (operation_status == MTPL_STATUS_KEY_REQUIRED) {
        *validation = MTPL_KEY_VALIDATION_REQUIRED;
        return MTPL_STATUS_KEY_REQUIRED;
    }
    if (mtpl_package_is_validation_failure(operation_status)) {
        *validation = MTPL_KEY_VALIDATION_REJECTED_OR_CORRUPT;
        return MTPL_STATUS_CRYPTO_ERROR;
    }
    return operation_status;
}

static const char *mtpl_package_magic_for_format(
    mtpl_package_format_t format) {
    switch (format) {
        case MTPL_PACKAGE_FORMAT_PTP: return mtpl_package_ptp_magic;
        case MTPL_PACKAGE_FORMAT_DTP: return mtpl_package_dtp_magic;
        case MTPL_PACKAGE_FORMAT_VTP: return mtpl_package_vtp_magic;
        default: return NULL;
    }
}

static mtpl_status_t mtpl_package_validate_tile_key(
    const char *path,
    mtpl_package_format_t format,
    const mtpl_crypto_options_t *crypto,
    mtpl_key_validation_t *validation) {
    mtpl_tile_core_reader_t *reader = NULL;
    const char *magic = mtpl_package_magic_for_format(format);
    size_t range_count = 0u;
    size_t range_index;
    mtpl_status_t status;

    if (crypto == NULL) {
        *validation = MTPL_KEY_VALIDATION_REQUIRED;
        return MTPL_STATUS_KEY_REQUIRED;
    }
    status = mtpl_tile_core_reader_open(path, magic, crypto, &reader);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_core_reader_get_range_count(reader, &range_count);
    }
    for (range_index = 0u;
         status == MTPL_STATUS_OK && range_index < range_count;
         ++range_index) {
        mtpl_tile_range_info_t range_info;
        size_t slot_index;
        status = mtpl_tile_core_reader_get_range_info(
            reader,
            range_index,
            &range_info);
        for (slot_index = 0u;
             status == MTPL_STATUS_OK && slot_index < range_info.slot_count;
             ++slot_index) {
            mtpl_tile_entry_info_t entry_info;
            mtpl_buffer_t tile = {NULL, 0u};
            status = mtpl_tile_core_reader_get_entry_info(
                reader,
                range_index,
                slot_index,
                &entry_info);
            if (status != MTPL_STATUS_OK || !entry_info.present) {
                continue;
            }
            status = mtpl_tile_core_reader_read_tile(
                reader,
                &entry_info.coordinate,
                &tile);
            mtpl_buffer_release(&tile);
            if (reader != NULL) {
                mtpl_status_t close_status =
                    mtpl_tile_core_reader_close(reader);
                reader = NULL;
                if (status == MTPL_STATUS_OK) {
                    status = close_status;
                }
            }
            return mtpl_package_validation_result(status, validation);
        }
    }
    if (reader != NULL) {
        mtpl_status_t close_status = mtpl_tile_core_reader_close(reader);
        reader = NULL;
        if (status == MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (status != MTPL_STATUS_OK) {
        return mtpl_package_validation_result(status, validation);
    }
    *validation = MTPL_KEY_VALIDATION_UNVERIFIABLE_EMPTY;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_package_validate_sfp_key(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_key_validation_t *validation) {
    mtpl_sfp_reader_t *reader = NULL;
    size_t entry_count = 0u;
    size_t entry_index;
    mtpl_status_t status;

    if (crypto == NULL) {
        *validation = MTPL_KEY_VALIDATION_REQUIRED;
        return MTPL_STATUS_KEY_REQUIRED;
    }
    status = mtpl_sfp_reader_open(path, crypto, &reader);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_reader_get_entry_count(reader, &entry_count);
    }
    for (entry_index = 0u;
         status == MTPL_STATUS_OK && entry_index < entry_count;
         ++entry_index) {
        mtpl_sfp_entry_info_t entry_info;
        mtpl_buffer_t data = {NULL, 0u};
        status = mtpl_sfp_reader_get_entry_info(
            reader,
            entry_index,
            &entry_info);
        if (status != MTPL_STATUS_OK ||
            entry_info.storage_mode != MTPL_STORAGE_ENCRYPTED) {
            continue;
        }
        status = mtpl_sfp_reader_read_file(reader, entry_info.path, &data);
        mtpl_buffer_release(&data);
        if (reader != NULL) {
            mtpl_status_t close_status = mtpl_sfp_reader_close(reader);
            reader = NULL;
            if (status == MTPL_STATUS_OK) {
                status = close_status;
            }
        }
        return mtpl_package_validation_result(status, validation);
    }
    if (reader != NULL) {
        mtpl_status_t close_status = mtpl_sfp_reader_close(reader);
        reader = NULL;
        if (status == MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (status != MTPL_STATUS_OK) {
        return mtpl_package_validation_result(status, validation);
    }
    *validation = MTPL_KEY_VALIDATION_UNVERIFIABLE_EMPTY;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_package_validate_key(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_key_validation_t *validation) {
    mtpl_package_info_t info = MTPL_PACKAGE_INFO_INIT;
    mtpl_status_t status;

    if (validation != NULL) {
        *validation = MTPL_KEY_VALIDATION_REJECTED_OR_CORRUPT;
    }
    if (path == NULL || path[0] == '\0' || validation == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_package_probe(path, &info);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (info.storage == MTPL_PACKAGE_STORAGE_PLAIN) {
        *validation = MTPL_KEY_VALIDATION_NOT_REQUIRED;
        return MTPL_STATUS_OK;
    }
    if (info.format == MTPL_PACKAGE_FORMAT_SFP) {
        return mtpl_package_validate_sfp_key(path, crypto, validation);
    }
    return mtpl_package_validate_tile_key(
        path,
        info.format,
        crypto,
        validation);
}

static bool mtpl_package_is_canceled(
    const mtpl_transcode_options_t *options) {
    return options->cancel_callback != NULL &&
        options->cancel_callback(options->user_data);
}

static void mtpl_package_report_progress(
    const mtpl_transcode_options_t *options,
    uint64_t completed,
    uint64_t total) {
    if (options->progress_callback != NULL) {
        options->progress_callback(completed, total, options->user_data);
    }
}

static mtpl_status_t mtpl_package_reserve_temporary_path(
    const char *destination_path,
    char **temporary_path) {
    size_t destination_size;
    size_t capacity;
    unsigned int attempt;
    bool destination_exists = false;
    mtpl_status_t status;

    if (temporary_path != NULL) {
        *temporary_path = NULL;
    }
    if (!mtpl_path_is_valid_utf8(destination_path) || temporary_path == NULL) {
        return destination_path != NULL && destination_path[0] != '\0'
            ? MTPL_STATUS_PATH_ERROR
            : MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_path_exists_utf8(destination_path, &destination_exists);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (destination_exists) {
        return MTPL_STATUS_ALREADY_EXISTS;
    }
    destination_size = strlen(destination_path);
    if (destination_size > SIZE_MAX - 48u) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    capacity = destination_size + 48u;
    *temporary_path = (char *)malloc(capacity);
    if (*temporary_path == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    for (attempt = 0u; attempt < MTPL_PACKAGE_TEMP_ATTEMPTS; ++attempt) {
        FILE *reservation;
        int written = snprintf(
            *temporary_path,
            capacity,
            "%s.mtpl-tmp-%u",
            destination_path,
            attempt);
        if (written < 0 || (size_t)written >= capacity) {
            free(*temporary_path);
            *temporary_path = NULL;
            return MTPL_STATUS_LIMIT_EXCEEDED;
        }
        reservation = mtpl_fopen_utf8(*temporary_path, "wbx");
        if (reservation != NULL) {
            if (fclose(reservation) != 0) {
                (void)mtpl_remove_utf8(*temporary_path);
                free(*temporary_path);
                *temporary_path = NULL;
                return MTPL_STATUS_IO_ERROR;
            }
            return MTPL_STATUS_OK;
        }
        if (errno != EEXIST) {
            free(*temporary_path);
            *temporary_path = NULL;
            return MTPL_STATUS_IO_ERROR;
        }
    }
    free(*temporary_path);
    *temporary_path = NULL;
    return MTPL_STATUS_ALREADY_EXISTS;
}

static mtpl_status_t mtpl_package_validate_destination_crypto(
    const mtpl_crypto_options_t *crypto) {
    mtpl_derived_key_t key = {NULL, 0u};
    mtpl_status_t status = mtpl_crypto_derive_key(crypto, &key);
    mtpl_crypto_key_release(&key);
    return status;
}

static mtpl_status_t mtpl_package_transcode_tile(
    const char *source_path,
    const char *temporary_path,
    mtpl_package_format_t format,
    const mtpl_transcode_options_t *options) {
    const char *magic = mtpl_package_magic_for_format(format);
    mtpl_tile_core_reader_t *reader = NULL;
    mtpl_tile_core_writer_t *writer = NULL;
    mtpl_buffer_t metadata = {NULL, 0u};
    mtpl_storage_mode_t destination_mode;
    uint32_t tile_size = 0u;
    size_t range_count = 0u;
    size_t range_index;
    uint64_t total = 0u;
    uint64_t completed = 0u;
    mtpl_status_t status;

    if (options->destination_storage == MTPL_PACKAGE_STORAGE_MIXED) {
        return MTPL_STATUS_UNSUPPORTED;
    }
    destination_mode = options->destination_storage ==
            MTPL_PACKAGE_STORAGE_ENCRYPTED
        ? MTPL_STORAGE_ENCRYPTED
        : MTPL_STORAGE_PLAIN;
    status = mtpl_tile_core_reader_open(
        source_path,
        magic,
        options->source_crypto,
        &reader);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_core_reader_get_tile_size(reader, &tile_size);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_core_reader_read_metadata(reader, &metadata);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_core_reader_get_range_count(reader, &range_count);
    }
    for (range_index = 0u;
         status == MTPL_STATUS_OK && range_index < range_count;
         ++range_index) {
        mtpl_tile_range_info_t range_info;
        status = mtpl_tile_core_reader_get_range_info(
            reader,
            range_index,
            &range_info);
        if (status == MTPL_STATUS_OK &&
            range_info.present_count > UINT64_MAX - total) {
            status = MTPL_STATUS_LIMIT_EXCEEDED;
        }
        if (status == MTPL_STATUS_OK) {
            total += (uint64_t)range_info.present_count;
        }
    }
    if (status == MTPL_STATUS_OK &&
        destination_mode == MTPL_STORAGE_ENCRYPTED) {
        status = mtpl_package_validate_destination_crypto(
            options->destination_crypto);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_tile_core_writer_create(
            temporary_path,
            magic,
            tile_size,
            (mtpl_buffer_view_t){metadata.data, metadata.size},
            destination_mode,
            options->destination_crypto,
            &writer);
    }
    mtpl_buffer_release(&metadata);
    mtpl_package_report_progress(options, 0u, total);
    if (status == MTPL_STATUS_OK && mtpl_package_is_canceled(options)) {
        status = MTPL_STATUS_CANCELED;
    }
    for (range_index = 0u;
         status == MTPL_STATUS_OK && range_index < range_count;
         ++range_index) {
        mtpl_tile_range_info_t range_info;
        status = mtpl_tile_core_reader_get_range_info(
            reader,
            range_index,
            &range_info);
        if (status == MTPL_STATUS_OK) {
            status = mtpl_tile_core_writer_add_range(
                writer,
                &range_info.bounds);
        }
    }
    for (range_index = 0u;
         status == MTPL_STATUS_OK && range_index < range_count;
         ++range_index) {
        mtpl_tile_range_info_t range_info;
        size_t slot_index;
        status = mtpl_tile_core_reader_get_range_info(
            reader,
            range_index,
            &range_info);
        for (slot_index = 0u;
             status == MTPL_STATUS_OK && slot_index < range_info.slot_count;
             ++slot_index) {
            mtpl_tile_entry_info_t entry_info;
            mtpl_buffer_t tile = {NULL, 0u};
            if (mtpl_package_is_canceled(options)) {
                status = MTPL_STATUS_CANCELED;
                break;
            }
            status = mtpl_tile_core_reader_get_entry_info(
                reader,
                range_index,
                slot_index,
                &entry_info);
            if (status != MTPL_STATUS_OK || !entry_info.present) {
                continue;
            }
            status = mtpl_tile_core_reader_read_tile(
                reader,
                &entry_info.coordinate,
                &tile);
            if (status == MTPL_STATUS_OK) {
                status = mtpl_tile_core_writer_add_tile_data(
                    writer,
                    &entry_info.coordinate,
                    (mtpl_buffer_view_t){tile.data, tile.size});
            }
            mtpl_buffer_release(&tile);
            if (status == MTPL_STATUS_OK) {
                ++completed;
                mtpl_package_report_progress(options, completed, total);
            }
        }
    }
    if (writer != NULL) {
        mtpl_status_t close_status = mtpl_tile_core_writer_close(writer);
        writer = NULL;
        if (status == MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (reader != NULL) {
        mtpl_status_t close_status = mtpl_tile_core_reader_close(reader);
        reader = NULL;
        if (status == MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (status == MTPL_STATUS_OK && mtpl_package_is_canceled(options)) {
        status = MTPL_STATUS_CANCELED;
    }
    return status;
}

static mtpl_status_t mtpl_package_transcode_sfp(
    const char *source_path,
    const char *temporary_path,
    const mtpl_transcode_options_t *options) {
    mtpl_sfp_reader_t *reader = NULL;
    mtpl_sfp_builder_t *builder = NULL;
    const mtpl_crypto_options_t *builder_crypto = NULL;
    bool needs_destination_crypto = false;
    size_t entry_count = 0u;
    size_t entry_index;
    mtpl_status_t status = mtpl_sfp_reader_open(
        source_path,
        options->source_crypto,
        &reader);

    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_reader_get_entry_count(reader, &entry_count);
    }
    for (entry_index = 0u;
         status == MTPL_STATUS_OK && entry_index < entry_count;
         ++entry_index) {
        mtpl_sfp_entry_info_t entry_info;
        status = mtpl_sfp_reader_get_entry_info(
            reader,
            entry_index,
            &entry_info);
        if (status == MTPL_STATUS_OK &&
            (options->destination_storage == MTPL_PACKAGE_STORAGE_ENCRYPTED ||
             (options->destination_storage == MTPL_PACKAGE_STORAGE_MIXED &&
              entry_info.storage_mode == MTPL_STORAGE_ENCRYPTED))) {
            needs_destination_crypto = true;
        }
    }
    if (status == MTPL_STATUS_OK && needs_destination_crypto) {
        status = mtpl_package_validate_destination_crypto(
            options->destination_crypto);
        builder_crypto = options->destination_crypto;
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_builder_create(builder_crypto, &builder);
    }
    mtpl_package_report_progress(options, 0u, (uint64_t)entry_count);
    if (status == MTPL_STATUS_OK && mtpl_package_is_canceled(options)) {
        status = MTPL_STATUS_CANCELED;
    }
    for (entry_index = 0u;
         status == MTPL_STATUS_OK && entry_index < entry_count;
         ++entry_index) {
        mtpl_sfp_entry_info_t entry_info = {
            NULL, 0u, 0u, MTPL_STORAGE_PLAIN
        };
        mtpl_storage_mode_t destination_mode;
        mtpl_buffer_t data = {NULL, 0u};
        if (mtpl_package_is_canceled(options)) {
            status = MTPL_STATUS_CANCELED;
            break;
        }
        status = mtpl_sfp_reader_get_entry_info(
            reader,
            entry_index,
            &entry_info);
        if (status == MTPL_STATUS_OK) {
            status = mtpl_sfp_reader_read_file(
                reader,
                entry_info.path,
                &data);
        }
        if (options->destination_storage == MTPL_PACKAGE_STORAGE_MIXED) {
            destination_mode = entry_info.storage_mode;
        } else {
            destination_mode = options->destination_storage ==
                    MTPL_PACKAGE_STORAGE_ENCRYPTED
                ? MTPL_STORAGE_ENCRYPTED
                : MTPL_STORAGE_PLAIN;
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_sfp_builder_add_data(
                builder,
                entry_info.path,
                (mtpl_buffer_view_t){data.data, data.size},
                destination_mode);
        }
        mtpl_buffer_release(&data);
    }
    if (reader != NULL) {
        mtpl_status_t close_status = mtpl_sfp_reader_close(reader);
        reader = NULL;
        if (status == MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    if (status == MTPL_STATUS_OK && mtpl_package_is_canceled(options)) {
        status = MTPL_STATUS_CANCELED;
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_builder_write_with_control(
            builder,
            temporary_path,
            options->progress_callback,
            options->cancel_callback,
            options->user_data);
    }
    mtpl_sfp_builder_destroy(builder);
    if (status == MTPL_STATUS_OK && mtpl_package_is_canceled(options)) {
        status = MTPL_STATUS_CANCELED;
    }
    return status;
}

mtpl_status_t mtpl_package_transcode(
    const char *source_path,
    const char *destination_path,
    const mtpl_transcode_options_t *options) {
    mtpl_package_info_t info = MTPL_PACKAGE_INFO_INIT;
    char *temporary_path = NULL;
    mtpl_status_t status;

    if (source_path == NULL || source_path[0] == '\0' ||
        destination_path == NULL || destination_path[0] == '\0' ||
        options == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (options->struct_size < sizeof(*options)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_path_is_valid_utf8(source_path) ||
        !mtpl_path_is_valid_utf8(destination_path)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    if (options->destination_storage != MTPL_PACKAGE_STORAGE_PLAIN &&
        options->destination_storage != MTPL_PACKAGE_STORAGE_ENCRYPTED &&
        options->destination_storage != MTPL_PACKAGE_STORAGE_MIXED) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_package_probe(source_path, &info);
    if (status == MTPL_STATUS_OK &&
        info.format != MTPL_PACKAGE_FORMAT_SFP &&
        options->destination_storage == MTPL_PACKAGE_STORAGE_MIXED) {
        status = MTPL_STATUS_UNSUPPORTED;
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_package_reserve_temporary_path(
            destination_path,
            &temporary_path);
    }
    if (status == MTPL_STATUS_OK && info.format == MTPL_PACKAGE_FORMAT_SFP) {
        status = mtpl_package_transcode_sfp(
            source_path,
            temporary_path,
            options);
    } else if (status == MTPL_STATUS_OK) {
        status = mtpl_package_transcode_tile(
            source_path,
            temporary_path,
            info.format,
            options);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_commit_file_utf8(temporary_path, destination_path);
    }
    if (temporary_path != NULL) {
        (void)mtpl_remove_utf8(temporary_path);
    }
    free(temporary_path);
    return status;
}
