#ifndef MTPL_SFP_INTERNAL_H
#define MTPL_SFP_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include <mtpl/sfp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mtpl_sfp_internal_progress_callback_t)(
    uint64_t completed,
    uint64_t total,
    void *user_data);

typedef bool (*mtpl_sfp_internal_cancel_callback_t)(void *user_data);

/*
 * Called synchronously with the next logical (decrypted and decompressed)
 * bytes of an SFP entry. The data is borrowed until the callback returns.
 * Any non-OK result stops the read and is propagated unchanged.
 */
typedef mtpl_status_t (*mtpl_sfp_internal_read_chunk_callback_t)(
    mtpl_buffer_view_t data,
    void *user_data);

/*
 * Internal bounded-memory reader used by in-tree consumers. chunk_size is a
 * nonzero callback upper bound and is capped internally at 1 MiB. Empty
 * entries invoke no callback. Chunks are provisional until the function
 * returns OK, and the callback must not reenter the same reader. Current
 * MTPL Snappy frames use a 32 KiB history window. A foreign frame requiring a
 * larger back-reference returns LIMIT_EXCEEDED.
 */
mtpl_status_t mtpl_sfp_reader_read_file_chunks_internal(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    size_t chunk_size,
    mtpl_sfp_internal_read_chunk_callback_t callback,
    void *user_data);

mtpl_status_t mtpl_sfp_builder_write_with_control(
    mtpl_sfp_builder_t *builder,
    const char *output_path,
    mtpl_sfp_internal_progress_callback_t progress_callback,
    mtpl_sfp_internal_cancel_callback_t cancel_callback,
    void *user_data);

#ifdef __cplusplus
}
#endif

#endif
