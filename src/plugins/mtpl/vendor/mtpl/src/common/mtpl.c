#include <stdlib.h>

#include <mtpl/mtpl.h>

const char *mtpl_version_string(void) {
    return MTPL_VERSION_STRING;
}

const char *mtpl_status_string(mtpl_status_t status) {
    switch (status) {
        case MTPL_STATUS_OK: return "success";
        case MTPL_STATUS_INVALID_ARGUMENT: return "invalid argument";
        case MTPL_STATUS_OUT_OF_MEMORY: return "out of memory";
        case MTPL_STATUS_IO_ERROR: return "I/O error";
        case MTPL_STATUS_FILE_NOT_FOUND: return "file not found";
        case MTPL_STATUS_FORMAT_ERROR: return "invalid file format";
        case MTPL_STATUS_FORMAT_MISMATCH: return "package format mismatch";
        case MTPL_STATUS_UNSUPPORTED_VERSION: return "unsupported format version";
        case MTPL_STATUS_KEY_REQUIRED: return "encryption key required";
        case MTPL_STATUS_CRYPTO_ERROR: return "encryption or decryption failed";
        case MTPL_STATUS_COMPRESSION_ERROR: return "compression failed";
        case MTPL_STATUS_DECOMPRESSION_ERROR: return "decompression failed";
        case MTPL_STATUS_NOT_FOUND: return "item not found";
        case MTPL_STATUS_ALREADY_EXISTS: return "item already exists";
        case MTPL_STATUS_OUT_OF_RANGE: return "value out of range";
        case MTPL_STATUS_CORRUPT_DATA: return "corrupt data";
        case MTPL_STATUS_PATH_ERROR: return "invalid path";
        case MTPL_STATUS_PATH_TRAVERSAL: return "path escapes destination";
        case MTPL_STATUS_LIMIT_EXCEEDED: return "format limit exceeded";
        case MTPL_STATUS_UNSUPPORTED: return "operation is unsupported";
        case MTPL_STATUS_INTERNAL_ERROR: return "internal error";
        case MTPL_STATUS_CANCELED: return "operation canceled";
        default: return "unknown status";
    }
}

void mtpl_buffer_release(mtpl_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }

    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
}
