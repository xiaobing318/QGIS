#ifndef MTPL_PACKAGE_H
#define MTPL_PACKAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <mtpl/export.h>
#include <mtpl/status.h>
#include <mtpl/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mtpl_package_format {
    MTPL_PACKAGE_FORMAT_UNKNOWN = 0,
    MTPL_PACKAGE_FORMAT_PTP = 1,
    MTPL_PACKAGE_FORMAT_DTP = 2,
    MTPL_PACKAGE_FORMAT_VTP = 3,
    MTPL_PACKAGE_FORMAT_SFP = 4
} mtpl_package_format_t;

typedef enum mtpl_package_storage {
    MTPL_PACKAGE_STORAGE_UNKNOWN = 0,
    MTPL_PACKAGE_STORAGE_PLAIN = 1,
    MTPL_PACKAGE_STORAGE_ENCRYPTED = 2,
    MTPL_PACKAGE_STORAGE_MIXED = 3
} mtpl_package_storage_t;

typedef struct mtpl_package_info {
    /* Set to sizeof(mtpl_package_info_t) before calling mtpl_package_probe. */
    uint32_t struct_size;
    mtpl_package_format_t format;
    mtpl_package_storage_t storage;
    uint32_t format_version;
    uint32_t tile_size;
    uint64_t file_size;
    uint64_t range_count;
    uint64_t entry_count;
    uint64_t reserved[4];
} mtpl_package_info_t;

#define MTPL_PACKAGE_INFO_INIT \
    { (uint32_t)sizeof(mtpl_package_info_t) }

typedef enum mtpl_key_validation {
    MTPL_KEY_VALIDATION_NOT_REQUIRED = 0,
    MTPL_KEY_VALIDATION_VALID = 1,
    MTPL_KEY_VALIDATION_REQUIRED = 2,
    MTPL_KEY_VALIDATION_REJECTED_OR_CORRUPT = 3,
    MTPL_KEY_VALIDATION_UNVERIFIABLE_EMPTY = 4
} mtpl_key_validation_t;

typedef void (*mtpl_transcode_progress_callback_t)(
    uint64_t completed,
    uint64_t total,
    void *user_data);

typedef bool (*mtpl_transcode_cancel_callback_t)(void *user_data);

typedef struct mtpl_transcode_options {
    /* Set to sizeof(mtpl_transcode_options_t) before calling transcode. */
    uint32_t struct_size;
    const mtpl_crypto_options_t *source_crypto;
    const mtpl_crypto_options_t *destination_crypto;
    mtpl_package_storage_t destination_storage;
    mtpl_transcode_progress_callback_t progress_callback;
    mtpl_transcode_cancel_callback_t cancel_callback;
    void *user_data;
    uint64_t reserved[4];
} mtpl_transcode_options_t;

#define MTPL_TRANSCODE_OPTIONS_INIT \
    { (uint32_t)sizeof(mtpl_transcode_options_t) }

/*
 * All paths are UTF-8. Format detection uses the package magic, not the suffix.
 * struct_size is an input capacity and is preserved. The implementation writes
 * only fields in its known structure size and ignores a larger caller tail.
 */
MTPL_API mtpl_status_t mtpl_package_probe(
    const char *path,
    mtpl_package_info_t *info);

MTPL_API mtpl_status_t mtpl_package_validate_key(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_key_validation_t *validation);

/*
 * Validation status/result pairs are:
 * OK/NOT_REQUIRED, KEY_REQUIRED/REQUIRED, OK/VALID,
 * CRYPTO_ERROR/REJECTED_OR_CORRUPT, and OK/UNVERIFIABLE_EMPTY.
 */

/*
 * Writes through a temporary file next to destination_path and commits only a
 * complete package. Existing destinations are never replaced.
 * MTPL_PACKAGE_STORAGE_MIXED preserves each SFP entry mode and is unsupported
 * for PTP, DTP, and VTP. options->struct_size must cover the 2.1 structure.
 */
MTPL_API mtpl_status_t mtpl_package_transcode(
    const char *source_path,
    const char *destination_path,
    const mtpl_transcode_options_t *options);

#ifdef __cplusplus
}
#endif

#endif
