#ifndef MTPL_SFP_H
#define MTPL_SFP_H

#include <stddef.h>
#include <stdint.h>

#include <mtpl/export.h>
#include <mtpl/status.h>
#include <mtpl/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mtpl_sfp_reader mtpl_sfp_reader_t;
typedef struct mtpl_sfp_list mtpl_sfp_list_t;
typedef struct mtpl_sfp_builder mtpl_sfp_builder_t;

typedef struct mtpl_sfp_entry_info {
    /* Borrowed UTF-8 path. It remains valid until the reader is closed. */
    const char *path;
    uint64_t logical_size;
    uint64_t stored_size;
    mtpl_storage_mode_t storage_mode;
} mtpl_sfp_entry_info_t;

MTPL_API mtpl_status_t mtpl_sfp_reader_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_sfp_reader_t **reader);

MTPL_API mtpl_status_t mtpl_sfp_reader_close(mtpl_sfp_reader_t *reader);

MTPL_API mtpl_status_t mtpl_sfp_reader_read_file(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    mtpl_buffer_t *buffer);

MTPL_API mtpl_status_t mtpl_sfp_reader_read_text(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    mtpl_buffer_t *buffer);

MTPL_API mtpl_status_t mtpl_sfp_reader_get_file_size(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    uint64_t *size);

MTPL_API mtpl_status_t mtpl_sfp_reader_get_entry_count(
    const mtpl_sfp_reader_t *reader,
    size_t *entry_count);

MTPL_API mtpl_status_t mtpl_sfp_reader_get_entry_info(
    const mtpl_sfp_reader_t *reader,
    size_t entry_index,
    mtpl_sfp_entry_info_t *info);

MTPL_API mtpl_status_t mtpl_sfp_reader_list(
    mtpl_sfp_reader_t *reader,
    const char *package_directory,
    mtpl_sfp_list_t **list);

MTPL_API mtpl_status_t mtpl_sfp_reader_list_json(
    mtpl_sfp_reader_t *reader,
    mtpl_buffer_t *json);

MTPL_API size_t mtpl_sfp_list_file_count(const mtpl_sfp_list_t *list);

MTPL_API mtpl_status_t mtpl_sfp_list_get_file(
    const mtpl_sfp_list_t *list,
    size_t index,
    const char **name,
    uint64_t *size);

MTPL_API size_t mtpl_sfp_list_directory_count(const mtpl_sfp_list_t *list);

MTPL_API mtpl_status_t mtpl_sfp_list_get_directory(
    const mtpl_sfp_list_t *list,
    size_t index,
    const char **name);

MTPL_API void mtpl_sfp_list_release(mtpl_sfp_list_t *list);

MTPL_API mtpl_status_t mtpl_sfp_builder_create(
    const mtpl_crypto_options_t *crypto,
    mtpl_sfp_builder_t **builder);

MTPL_API mtpl_status_t mtpl_sfp_builder_add_file(
    mtpl_sfp_builder_t *builder,
    const char *package_path,
    const char *source_path,
    mtpl_storage_mode_t mode);

MTPL_API mtpl_status_t mtpl_sfp_builder_add_data(
    mtpl_sfp_builder_t *builder,
    const char *package_path,
    mtpl_buffer_view_t data,
    mtpl_storage_mode_t mode);

MTPL_API mtpl_status_t mtpl_sfp_builder_write(
    mtpl_sfp_builder_t *builder,
    const char *output_path);

MTPL_API void mtpl_sfp_builder_destroy(mtpl_sfp_builder_t *builder);

MTPL_API mtpl_status_t mtpl_sfp_pack_directory(
    const char *input_directory,
    const char *output_path,
    mtpl_storage_mode_t mode,
    const mtpl_crypto_options_t *crypto);

MTPL_API mtpl_status_t mtpl_sfp_unpack(
    const char *input_path,
    const char *output_directory,
    const mtpl_crypto_options_t *crypto,
    uint32_t *file_count);

#ifdef __cplusplus
}
#endif

#endif
