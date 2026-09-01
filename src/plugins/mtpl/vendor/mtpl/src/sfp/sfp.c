#include <mtpl/sfp.h>

#include "sfp_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "../internal/crypto.h"
#include "../internal/file_io.h"
#include "../security/arc4.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define MTPL_SFP_MAGIC "SFP"
#define MTPL_SFP_MAGIC_SIZE 4u
#define MTPL_SFP_VERSION 2u
#define MTPL_SFP_FIXED_HEADER_SIZE 16u
#define MTPL_SFP_INDEX_ENTRY_SIZE 24u
#define MTPL_SFP_MAX_FILE_COUNT 100000u
#define MTPL_SFP_MAX_RECURSION_DEPTH 128u
#define MTPL_SFP_STREAM_INPUT_SIZE (64u * 1024u)
#define MTPL_SFP_STREAM_MAX_CHUNK_SIZE (1024u * 1024u)
#define MTPL_SFP_STREAM_SNAPPY_HISTORY_SIZE (32u * 1024u)
#define MTPL_SFP_CRYPTO_FRAME_MAGIC "dTd"
#define MTPL_SFP_CRYPTO_FRAME_VERSION 1u
#define MTPL_SFP_CRYPTO_HEADER_SIZE 20u
#define MTPL_SFP_CRYPTO_XOR_OFFSET 18u
#define MTPL_SFP_CRYPTO_XOR_LIMIT 1024u

typedef struct mtpl_sfp_entry {
    uint64_t id;
    uint64_t record_offset;
    uint64_t payload_offset;
    uint32_t original_size;
    uint32_t encrypted_size;
    char *path;
    uint16_t path_size;
} mtpl_sfp_entry_t;

struct mtpl_sfp_reader {
    FILE *file;
    uint64_t file_size;
    mtpl_sfp_entry_t *entries;
    size_t entry_count;
    mtpl_derived_key_t key;
};

typedef struct mtpl_sfp_list_file {
    char *name;
    uint64_t size;
} mtpl_sfp_list_file_t;

struct mtpl_sfp_list {
    mtpl_sfp_list_file_t *files;
    size_t file_count;
    char **directories;
    size_t directory_count;
};

typedef struct mtpl_sfp_builder_entry {
    char *package_path;
    char *source_path;
    uint8_t *data;
    uint32_t size;
    uint32_t encrypted_size;
    uint64_t id;
    uint64_t record_offset;
    bool source_is_file;
    mtpl_storage_mode_t mode;
} mtpl_sfp_builder_entry_t;

struct mtpl_sfp_builder {
    mtpl_sfp_builder_entry_t *entries;
    size_t entry_count;
    size_t entry_capacity;
    mtpl_derived_key_t key;
};

typedef struct mtpl_sfp_string {
    uint8_t *data;
    size_t size;
    size_t capacity;
} mtpl_sfp_string_t;

typedef struct mtpl_sfp_stream_output {
    uint8_t *buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    uint8_t *history;
    size_t history_capacity;
    uint64_t produced;
    uint64_t expected;
    mtpl_sfp_internal_read_chunk_callback_t callback;
    void *user_data;
    mtpl_status_t callback_status;
} mtpl_sfp_stream_output_t;

typedef struct mtpl_sfp_frame_cursor {
    FILE *file;
    stream_state cipher;
    uint64_t frame_offset;
    uint64_t encrypted_end;
    uint64_t disk_remaining;
    uint8_t *input;
    size_t input_offset;
    size_t input_size;
} mtpl_sfp_frame_cursor_t;

typedef struct mtpl_sfp_frame_info {
    mtpl_compression_method_t compression_method;
    uint32_t original_size;
} mtpl_sfp_frame_info_t;

static void mtpl_sfp_zero_buffer(mtpl_buffer_t *buffer) {
    if (buffer != NULL) {
        buffer->data = NULL;
        buffer->size = 0;
    }
}

static void mtpl_sfp_release_buffer(mtpl_buffer_t *buffer) {
    if (buffer != NULL) {
        free(buffer->data);
        mtpl_sfp_zero_buffer(buffer);
    }
}

static char *mtpl_sfp_strdup(const char *value) {
    size_t size;
    char *copy;

    if (value == NULL) {
        return NULL;
    }
    size = strlen(value);
    if (size == SIZE_MAX) {
        return NULL;
    }
    copy = (char *)malloc(size + 1u);
    if (copy != NULL) {
        memcpy(copy, value, size + 1u);
    }
    return copy;
}

static uint16_t mtpl_sfp_read_u16(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
}

static uint32_t mtpl_sfp_read_u32(const uint8_t *data) {
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8u) |
        ((uint32_t)data[2] << 16u) |
        ((uint32_t)data[3] << 24u);
}

static uint64_t mtpl_sfp_read_u64(const uint8_t *data) {
    return (uint64_t)mtpl_sfp_read_u32(data) |
        ((uint64_t)mtpl_sfp_read_u32(data + 4u) << 32u);
}

static void mtpl_sfp_write_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}

static void mtpl_sfp_write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void mtpl_sfp_write_u64(uint8_t *data, uint64_t value) {
    mtpl_sfp_write_u32(data, (uint32_t)value);
    mtpl_sfp_write_u32(data + 4u, (uint32_t)(value >> 32u));
}

static bool mtpl_sfp_add_overflows_u64(uint64_t left, uint64_t right) {
    return right > UINT64_MAX - left;
}

static mtpl_status_t mtpl_sfp_read_exact(FILE *file, void *data, size_t size) {
    uint8_t *output = (uint8_t *)data;
    size_t completed = 0u;

    while (completed < size) {
        size_t current = fread(output + completed, 1u, size - completed, file);
        if (current == 0u) {
            return ferror(file) != 0
                ? MTPL_STATUS_IO_ERROR
                : MTPL_STATUS_CORRUPT_DATA;
        }
        completed += current;
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_write_exact(FILE *file, const void *data, size_t size) {
    const uint8_t *input = (const uint8_t *)data;
    size_t completed = 0u;

    while (completed < size) {
        size_t current = fwrite(input + completed, 1u, size - completed, file);
        if (current == 0u) {
            return MTPL_STATUS_IO_ERROR;
        }
        completed += current;
    }
    return MTPL_STATUS_OK;
}

static bool mtpl_sfp_segment_is_dot(const char *segment, size_t size) {
    return (size == 1u && segment[0] == '.') ||
        (size == 2u && segment[0] == '.' && segment[1] == '.');
}

static unsigned char mtpl_sfp_ascii_lower(unsigned char value) {
    return value >= (unsigned char)'A' && value <= (unsigned char)'Z'
        ? (unsigned char)(value + ((unsigned char)'a' - (unsigned char)'A'))
        : value;
}

static bool mtpl_sfp_ascii_equal_n(
    const char *left,
    const char *right,
    size_t size) {
    size_t index;
    for (index = 0u; index < size; ++index) {
        if (mtpl_sfp_ascii_lower((unsigned char)left[index]) !=
            mtpl_sfp_ascii_lower((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

static bool mtpl_sfp_segment_is_windows_device(
    const char *segment,
    size_t size) {
    size_t base_size = 0u;

    while (base_size < size && segment[base_size] != '.') {
        ++base_size;
    }
    if (base_size == 3u &&
        (mtpl_sfp_ascii_equal_n(segment, "con", 3u) ||
         mtpl_sfp_ascii_equal_n(segment, "prn", 3u) ||
         mtpl_sfp_ascii_equal_n(segment, "aux", 3u) ||
         mtpl_sfp_ascii_equal_n(segment, "nul", 3u))) {
        return true;
    }
    if (base_size == 4u &&
        (mtpl_sfp_ascii_equal_n(segment, "com", 3u) ||
         mtpl_sfp_ascii_equal_n(segment, "lpt", 3u)) &&
        segment[3] >= '1' && segment[3] <= '9') {
        return true;
    }
    return (base_size == 6u &&
            mtpl_sfp_ascii_equal_n(segment, "conin$", 6u)) ||
        (base_size == 7u &&
         mtpl_sfp_ascii_equal_n(segment, "conout$", 7u));
}

static bool mtpl_sfp_paths_collide_on_windows(
    const char *left,
    const char *right) {
    size_t index = 0u;
    while (left[index] != '\0' && right[index] != '\0') {
        if (mtpl_sfp_ascii_lower((unsigned char)left[index]) !=
            mtpl_sfp_ascii_lower((unsigned char)right[index])) {
            return false;
        }
        ++index;
    }
    return left[index] == right[index];
}

#ifdef _WIN32
static bool mtpl_sfp_path_is_windows_output_compatible(const char *path) {
    size_t index;
    for (index = 0u; path[index] != '\0'; ++index) {
        unsigned char value = (unsigned char)path[index];
        if (value < 0x20u || value == '"' || value == '*' ||
            value == '<' || value == '>' || value == '?' || value == '|') {
            return false;
        }
    }
    return true;
}
#endif

static mtpl_status_t mtpl_sfp_normalize_package_path(
    const char *input,
    bool allow_empty,
    bool allow_trailing_separator,
    char **output) {
    char *normalized;
    size_t input_size;
    size_t size;
    size_t index;
    size_t segment_start;

    if (output == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *output = NULL;
    if (input == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    input_size = strlen(input);
    if (!mtpl_utf8_is_valid((const uint8_t *)input, input_size)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    normalized = (char *)malloc(input_size + 1u);
    if (normalized == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    for (index = 0; index < input_size; ++index) {
        char value = input[index];
        normalized[index] = value == '\\' ? '/' : value;
    }
    normalized[input_size] = '\0';
    size = input_size;
    if (allow_trailing_separator) {
        while (size > 0u && normalized[size - 1u] == '/') {
            normalized[--size] = '\0';
        }
    }
    if (size == 0u) {
        if (!allow_empty) {
            free(normalized);
            return MTPL_STATUS_PATH_ERROR;
        }
        *output = normalized;
        return MTPL_STATUS_OK;
    }
    if (normalized[0] == '/' || normalized[size - 1u] == '/') {
        free(normalized);
        return MTPL_STATUS_PATH_ERROR;
    }
    segment_start = 0u;
    for (index = 0; index <= size; ++index) {
        if (index < size && normalized[index] == ':') {
            free(normalized);
            return MTPL_STATUS_PATH_TRAVERSAL;
        }
        if (index == size || normalized[index] == '/') {
            size_t segment_size = index - segment_start;
            size_t windows_size = segment_size;
            if (segment_size == 0u ||
                mtpl_sfp_segment_is_dot(normalized + segment_start, segment_size)) {
                free(normalized);
                return segment_size == 2u
                    ? MTPL_STATUS_PATH_TRAVERSAL
                    : MTPL_STATUS_PATH_ERROR;
            }
            while (windows_size > 0u &&
                   (normalized[segment_start + windows_size - 1u] == '.' ||
                    normalized[segment_start + windows_size - 1u] == ' ')) {
                --windows_size;
            }
            if (windows_size != segment_size) {
                bool aliases_dot = mtpl_sfp_segment_is_dot(
                    normalized + segment_start,
                    windows_size);
                free(normalized);
                return aliases_dot
                    ? MTPL_STATUS_PATH_TRAVERSAL
                    : MTPL_STATUS_PATH_ERROR;
            }
            if (mtpl_sfp_segment_is_windows_device(
                    normalized + segment_start,
                    segment_size)) {
                free(normalized);
                return MTPL_STATUS_PATH_ERROR;
            }
            segment_start = index + 1u;
        }
    }
    *output = normalized;
    return MTPL_STATUS_OK;
}

static uint64_t mtpl_sfp_path_id(const char *path) {
    return (uint64_t)(uint32_t)crc32(
        0L,
        (const Bytef *)path,
        (uInt)strlen(path));
}

static uint64_t mtpl_sfp_entry_stored_size(const mtpl_sfp_entry_t *entry) {
    return entry->encrypted_size != 0u
        ? entry->encrypted_size
        : entry->original_size;
}

static void mtpl_sfp_entry_release(mtpl_sfp_entry_t *entry) {
    if (entry != NULL) {
        free(entry->path);
        memset(entry, 0, sizeof(*entry));
    }
}

static int mtpl_sfp_compare_entry_path(const void *left, const void *right) {
    const mtpl_sfp_entry_t *left_entry = (const mtpl_sfp_entry_t *)left;
    const mtpl_sfp_entry_t *right_entry = (const mtpl_sfp_entry_t *)right;
    return strcmp(left_entry->path, right_entry->path);
}

static int mtpl_sfp_compare_entry_pointer_id(const void *left, const void *right) {
    const mtpl_sfp_entry_t *const *left_entry =
        (const mtpl_sfp_entry_t *const *)left;
    const mtpl_sfp_entry_t *const *right_entry =
        (const mtpl_sfp_entry_t *const *)right;
    if ((*left_entry)->id < (*right_entry)->id) {
        return -1;
    }
    if ((*left_entry)->id > (*right_entry)->id) {
        return 1;
    }
    return 0;
}

static int mtpl_sfp_compare_entry_pointer_offset(const void *left, const void *right) {
    const mtpl_sfp_entry_t *const *left_entry =
        (const mtpl_sfp_entry_t *const *)left;
    const mtpl_sfp_entry_t *const *right_entry =
        (const mtpl_sfp_entry_t *const *)right;
    if ((*left_entry)->record_offset < (*right_entry)->record_offset) {
        return -1;
    }
    if ((*left_entry)->record_offset > (*right_entry)->record_offset) {
        return 1;
    }
    return 0;
}

static int mtpl_sfp_compare_entry_pointer_windows_path(
    const void *left,
    const void *right) {
    const mtpl_sfp_entry_t *const *left_entry =
        (const mtpl_sfp_entry_t *const *)left;
    const mtpl_sfp_entry_t *const *right_entry =
        (const mtpl_sfp_entry_t *const *)right;
    const char *left_path = (*left_entry)->path;
    const char *right_path = (*right_entry)->path;
    size_t index = 0u;

    while (left_path[index] != '\0' && right_path[index] != '\0') {
        unsigned char left_value =
            mtpl_sfp_ascii_lower((unsigned char)left_path[index]);
        unsigned char right_value =
            mtpl_sfp_ascii_lower((unsigned char)right_path[index]);
        if (left_value < right_value) {
            return -1;
        }
        if (left_value > right_value) {
            return 1;
        }
        ++index;
    }
    if (left_path[index] == '\0' && right_path[index] != '\0') {
        return -1;
    }
    if (left_path[index] != '\0' && right_path[index] == '\0') {
        return 1;
    }
    return strcmp(left_path, right_path);
}

static mtpl_sfp_entry_t *mtpl_sfp_find_entry(
    mtpl_sfp_reader_t *reader,
    const char *path) {
    size_t low = 0u;
    size_t high = reader->entry_count;

    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        int comparison = strcmp(path, reader->entries[middle].path);
        if (comparison == 0) {
            return &reader->entries[middle];
        }
        if (comparison < 0) {
            high = middle;
        } else {
            low = middle + 1u;
        }
    }
    return NULL;
}

static mtpl_status_t mtpl_sfp_validate_entries(
    mtpl_sfp_reader_t *reader,
    uint64_t index_end) {
    mtpl_sfp_entry_t **ordered;
    size_t index;

    if (reader->entry_count == 0u) {
        return MTPL_STATUS_OK;
    }
    ordered = (mtpl_sfp_entry_t **)malloc(
        reader->entry_count * sizeof(*ordered));
    if (ordered == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    for (index = 0u; index < reader->entry_count; ++index) {
        ordered[index] = &reader->entries[index];
    }

    qsort(
        ordered,
        reader->entry_count,
        sizeof(*ordered),
        mtpl_sfp_compare_entry_pointer_id);
    for (index = 1u; index < reader->entry_count; ++index) {
        if (ordered[index - 1u]->id == ordered[index]->id) {
            free(ordered);
            return MTPL_STATUS_CORRUPT_DATA;
        }
    }

    qsort(
        ordered,
        reader->entry_count,
        sizeof(*ordered),
        mtpl_sfp_compare_entry_pointer_offset);
    for (index = 0u; index < reader->entry_count; ++index) {
        const mtpl_sfp_entry_t *entry = ordered[index];
        uint64_t stored_size = mtpl_sfp_entry_stored_size(entry);
        uint64_t record_end = entry->payload_offset + stored_size;

        if (entry->record_offset < index_end || record_end > reader->file_size) {
            free(ordered);
            return MTPL_STATUS_CORRUPT_DATA;
        }
        if (index != 0u) {
            const mtpl_sfp_entry_t *previous = ordered[index - 1u];
            uint64_t previous_end = previous->payload_offset +
                mtpl_sfp_entry_stored_size(previous);
            if (entry->record_offset < previous_end) {
                free(ordered);
                return MTPL_STATUS_CORRUPT_DATA;
            }
        }
    }

    qsort(
        ordered,
        reader->entry_count,
        sizeof(*ordered),
        mtpl_sfp_compare_entry_pointer_windows_path);
    for (index = 1u; index < reader->entry_count; ++index) {
        if (mtpl_sfp_paths_collide_on_windows(
                ordered[index - 1u]->path,
                ordered[index]->path)) {
            free(ordered);
            return MTPL_STATUS_CORRUPT_DATA;
        }
    }
    free(ordered);

    qsort(
        reader->entries,
        reader->entry_count,
        sizeof(*reader->entries),
        mtpl_sfp_compare_entry_path);
    for (index = 1u; index < reader->entry_count; ++index) {
        if (strcmp(
                reader->entries[index - 1u].path,
                reader->entries[index].path) == 0) {
            return MTPL_STATUS_CORRUPT_DATA;
        }
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_parse_reader(mtpl_sfp_reader_t *reader) {
    uint8_t fixed_header[MTPL_SFP_FIXED_HEADER_SIZE];
    uint32_t file_count;
    uint64_t index_size;
    uint64_t index_end;
    size_t index;
    mtpl_status_t status;

    status = mtpl_file_get_size(reader->file, &reader->file_size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (reader->file_size < MTPL_SFP_MAGIC_SIZE) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    status = mtpl_file_seek(reader->file, 0u);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_sfp_read_exact(
        reader->file,
        fixed_header,
        MTPL_SFP_MAGIC_SIZE);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (memcmp(fixed_header, MTPL_SFP_MAGIC "\0", MTPL_SFP_MAGIC_SIZE) != 0) {
        return MTPL_STATUS_FORMAT_MISMATCH;
    }
    if (reader->file_size < 8u) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    status = mtpl_sfp_read_exact(reader->file, fixed_header + 4u, 4u);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (mtpl_sfp_read_u32(fixed_header + 4u) != MTPL_SFP_VERSION) {
        return MTPL_STATUS_UNSUPPORTED_VERSION;
    }
    if (reader->file_size < MTPL_SFP_FIXED_HEADER_SIZE) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    status = mtpl_sfp_read_exact(reader->file, fixed_header + 8u, 8u);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    file_count = mtpl_sfp_read_u32(fixed_header + 8u);
    if (file_count > MTPL_SFP_MAX_FILE_COUNT) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    index_size = (uint64_t)file_count * MTPL_SFP_INDEX_ENTRY_SIZE;
    index_end = MTPL_SFP_FIXED_HEADER_SIZE + index_size;
    if (index_end > reader->file_size) {
        return MTPL_STATUS_CORRUPT_DATA;
    }

    if (file_count != 0u) {
        reader->entries = (mtpl_sfp_entry_t *)calloc(
            file_count,
            sizeof(*reader->entries));
        if (reader->entries == NULL) {
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
    }
    reader->entry_count = file_count;

    for (index = 0u; index < file_count; ++index) {
        uint8_t record[MTPL_SFP_INDEX_ENTRY_SIZE];
        mtpl_sfp_entry_t *entry = &reader->entries[index];
        uint8_t path_size_data[2];
        char *normalized = NULL;
        uint64_t stored_size;

        status = mtpl_file_seek(
            reader->file,
            MTPL_SFP_FIXED_HEADER_SIZE +
                (uint64_t)index * MTPL_SFP_INDEX_ENTRY_SIZE);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        status = mtpl_sfp_read_exact(reader->file, record, sizeof(record));
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        entry->id = mtpl_sfp_read_u64(record);
        entry->record_offset = mtpl_sfp_read_u64(record + 8u);
        entry->original_size = mtpl_sfp_read_u32(record + 16u);
        entry->encrypted_size = mtpl_sfp_read_u32(record + 20u);
        stored_size = mtpl_sfp_entry_stored_size(entry);

        if (entry->encrypted_size != 0u && entry->encrypted_size < 20u) {
            return MTPL_STATUS_CORRUPT_DATA;
        }
        if (entry->record_offset < index_end ||
            mtpl_sfp_add_overflows_u64(entry->record_offset, 2u) ||
            entry->record_offset + 2u > reader->file_size) {
            return MTPL_STATUS_CORRUPT_DATA;
        }
        status = mtpl_file_seek(reader->file, entry->record_offset);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        status = mtpl_sfp_read_exact(reader->file, path_size_data, 2u);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        entry->path_size = mtpl_sfp_read_u16(path_size_data);
        if (entry->path_size == 0u ||
            mtpl_sfp_add_overflows_u64(entry->record_offset + 2u, entry->path_size) ||
            mtpl_sfp_add_overflows_u64(
                entry->record_offset + 2u + entry->path_size,
                stored_size)) {
            return MTPL_STATUS_CORRUPT_DATA;
        }
        entry->payload_offset = entry->record_offset + 2u + entry->path_size;
        if (entry->payload_offset + stored_size > reader->file_size) {
            return MTPL_STATUS_CORRUPT_DATA;
        }
        entry->path = (char *)malloc((size_t)entry->path_size + 1u);
        if (entry->path == NULL) {
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        status = mtpl_sfp_read_exact(
            reader->file,
            entry->path,
            entry->path_size);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        if (memchr(entry->path, '\0', entry->path_size) != NULL) {
            return MTPL_STATUS_CORRUPT_DATA;
        }
        entry->path[entry->path_size] = '\0';
        status = mtpl_sfp_normalize_package_path(
            entry->path,
            false,
            false,
            &normalized);
        if (status != MTPL_STATUS_OK) {
            return status == MTPL_STATUS_OUT_OF_MEMORY
                ? status
                : MTPL_STATUS_CORRUPT_DATA;
        }
        if (strcmp(entry->path, normalized) != 0 ||
            entry->id != mtpl_sfp_path_id(normalized)) {
            free(normalized);
            return MTPL_STATUS_CORRUPT_DATA;
        }
        free(normalized);
    }
    return mtpl_sfp_validate_entries(reader, index_end);
}

mtpl_status_t mtpl_sfp_reader_open(
    const char *path,
    const mtpl_crypto_options_t *crypto,
    mtpl_sfp_reader_t **reader) {
    mtpl_sfp_reader_t *created;
    mtpl_status_t status;

    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *reader = NULL;
    if (path == NULL || path[0] == '\0') {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_utf8_is_valid((const uint8_t *)path, strlen(path))) {
        return MTPL_STATUS_PATH_ERROR;
    }
    created = (mtpl_sfp_reader_t *)calloc(1u, sizeof(*created));
    if (created == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    created->file = mtpl_fopen_utf8(path, "rb");
    if (created->file == NULL) {
        free(created);
        return errno == ENOENT
            ? MTPL_STATUS_FILE_NOT_FOUND
            : MTPL_STATUS_IO_ERROR;
    }
    status = mtpl_sfp_parse_reader(created);
    if (status == MTPL_STATUS_OK && crypto != NULL) {
        status = mtpl_crypto_derive_key(crypto, &created->key);
    }
    if (status != MTPL_STATUS_OK) {
        mtpl_sfp_reader_close(created);
        return status;
    }
    *reader = created;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_sfp_reader_close(mtpl_sfp_reader_t *reader) {
    size_t index;
    mtpl_status_t status = MTPL_STATUS_OK;

    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (reader->file != NULL && fclose(reader->file) != 0) {
        status = MTPL_STATUS_IO_ERROR;
    }
    reader->file = NULL;
    for (index = 0u; index < reader->entry_count; ++index) {
        mtpl_sfp_entry_release(&reader->entries[index]);
    }
    free(reader->entries);
    mtpl_crypto_key_release(&reader->key);
    free(reader);
    return status;
}

mtpl_status_t mtpl_sfp_reader_get_entry_count(
    const mtpl_sfp_reader_t *reader,
    size_t *entry_count) {
    if (entry_count != NULL) {
        *entry_count = 0u;
    }
    if (reader == NULL || entry_count == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *entry_count = reader->entry_count;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_sfp_reader_get_entry_info(
    const mtpl_sfp_reader_t *reader,
    size_t entry_index,
    mtpl_sfp_entry_info_t *info) {
    const mtpl_sfp_entry_t *entry;

    if (info != NULL) {
        memset(info, 0, sizeof(*info));
        info->storage_mode = MTPL_STORAGE_PLAIN;
    }
    if (reader == NULL || info == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (entry_index >= reader->entry_count) {
        return MTPL_STATUS_OUT_OF_RANGE;
    }
    entry = &reader->entries[entry_index];
    info->path = entry->path;
    info->logical_size = entry->original_size;
    info->stored_size = mtpl_sfp_entry_stored_size(entry);
    info->storage_mode = entry->encrypted_size == 0u
        ? MTPL_STORAGE_PLAIN
        : MTPL_STORAGE_ENCRYPTED;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_lookup_normalized(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    mtpl_sfp_entry_t **entry) {
    char *normalized = NULL;
    mtpl_status_t status;

    if (reader == NULL || entry == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *entry = NULL;
    status = mtpl_sfp_normalize_package_path(
        package_path,
        false,
        false,
        &normalized);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    *entry = mtpl_sfp_find_entry(reader, normalized);
    free(normalized);
    return *entry == NULL ? MTPL_STATUS_NOT_FOUND : MTPL_STATUS_OK;
}

static void mtpl_sfp_stream_secure_free(uint8_t *data, size_t size) {
    if (data != NULL) {
        memset(data, 0, size);
        free(data);
    }
}

static mtpl_status_t mtpl_sfp_stream_output_init(
    mtpl_sfp_stream_output_t *output,
    uint64_t expected,
    size_t chunk_size,
    size_t history_capacity,
    mtpl_sfp_internal_read_chunk_callback_t callback,
    void *user_data) {
    size_t effective_chunk_size = chunk_size;

    memset(output, 0, sizeof(*output));
    if (effective_chunk_size > MTPL_SFP_STREAM_MAX_CHUNK_SIZE) {
        effective_chunk_size = MTPL_SFP_STREAM_MAX_CHUNK_SIZE;
    }
    output->buffer = (uint8_t *)malloc(effective_chunk_size);
    if (output->buffer == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    if (history_capacity != 0u) {
        output->history = (uint8_t *)malloc(history_capacity);
        if (output->history == NULL) {
            mtpl_sfp_stream_secure_free(
                output->buffer,
                effective_chunk_size);
            memset(output, 0, sizeof(*output));
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
    }
    output->buffer_capacity = effective_chunk_size;
    output->history_capacity = history_capacity;
    output->expected = expected;
    output->callback = callback;
    output->user_data = user_data;
    output->callback_status = MTPL_STATUS_OK;
    return MTPL_STATUS_OK;
}

static void mtpl_sfp_stream_output_release(
    mtpl_sfp_stream_output_t *output) {
    if (output == NULL) {
        return;
    }
    mtpl_sfp_stream_secure_free(output->buffer, output->buffer_capacity);
    mtpl_sfp_stream_secure_free(output->history, output->history_capacity);
    memset(output, 0, sizeof(*output));
}

static mtpl_status_t mtpl_sfp_stream_output_flush(
    mtpl_sfp_stream_output_t *output) {
    mtpl_status_t status;

    if (output->buffer_size == 0u) {
        return MTPL_STATUS_OK;
    }
    status = output->callback(
        (mtpl_buffer_view_t){output->buffer, output->buffer_size},
        output->user_data);
    if (status != MTPL_STATUS_OK) {
        output->callback_status = status;
        return status;
    }
    memset(output->buffer, 0, output->buffer_size);
    output->buffer_size = 0u;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_stream_output_write(
    mtpl_sfp_stream_output_t *output,
    const uint8_t *data,
    size_t size) {
    while (size != 0u) {
        size_t available = output->buffer_capacity - output->buffer_size;
        size_t current = size < available ? size : available;

        if ((uint64_t)current > output->expected - output->produced) {
            return MTPL_STATUS_DECOMPRESSION_ERROR;
        }
        if (output->history_capacity != 0u) {
            size_t history_offset =
                (size_t)(output->produced % output->history_capacity);
            size_t history_available =
                output->history_capacity - history_offset;
            if (current > history_available) {
                current = history_available;
            }
            memcpy(output->history + history_offset, data, current);
        }
        memcpy(output->buffer + output->buffer_size, data, current);
        output->buffer_size += current;
        output->produced += current;
        data += current;
        size -= current;
        if (output->buffer_size == output->buffer_capacity) {
            mtpl_status_t status = mtpl_sfp_stream_output_flush(output);
            if (status != MTPL_STATUS_OK) {
                return status;
            }
        }
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_stream_output_copy(
    mtpl_sfp_stream_output_t *output,
    uint32_t offset,
    uint32_t size) {
    uint32_t index;

    if (offset == 0u || offset > output->produced) {
        return MTPL_STATUS_DECOMPRESSION_ERROR;
    }
    if (offset > output->history_capacity) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    for (index = 0u; index < size; ++index) {
        size_t history_offset = (size_t)(
            (output->produced - offset) % output->history_capacity);
        uint8_t value = output->history[history_offset];
        mtpl_status_t status = mtpl_sfp_stream_output_write(
            output,
            &value,
            1u);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_stream_output_finish(
    mtpl_sfp_stream_output_t *output) {
    if (output->produced != output->expected) {
        return MTPL_STATUS_DECOMPRESSION_ERROR;
    }
    return mtpl_sfp_stream_output_flush(output);
}

static mtpl_status_t mtpl_sfp_frame_read_direct(
    mtpl_sfp_frame_cursor_t *cursor,
    uint8_t *data,
    size_t size) {
    size_t encrypted_size = 0u;
    mtpl_status_t status;

    if ((uint64_t)size > cursor->disk_remaining) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    status = mtpl_sfp_read_exact(cursor->file, data, size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (cursor->frame_offset < cursor->encrypted_end) {
        uint64_t available = cursor->encrypted_end - cursor->frame_offset;
        encrypted_size = available < size ? (size_t)available : size;
        stream_encrypt(&cursor->cipher, data, encrypted_size);
    }
    cursor->frame_offset += size;
    cursor->disk_remaining -= size;
    return MTPL_STATUS_OK;
}

static void mtpl_sfp_frame_cursor_release(
    mtpl_sfp_frame_cursor_t *cursor) {
    if (cursor == NULL) {
        return;
    }
    mtpl_sfp_stream_secure_free(cursor->input, MTPL_SFP_STREAM_INPUT_SIZE);
    memset(cursor, 0, sizeof(*cursor));
}

static mtpl_status_t mtpl_sfp_frame_cursor_init(
    mtpl_sfp_reader_t *reader,
    const mtpl_sfp_entry_t *entry,
    mtpl_sfp_frame_cursor_t *cursor,
    mtpl_sfp_frame_info_t *info) {
    uint8_t header[MTPL_SFP_CRYPTO_HEADER_SIZE];
    uint8_t discard[4096];
    uint64_t stored_size = mtpl_sfp_entry_stored_size(entry);
    uint16_t xor_size;
    uint16_t data_offset;
    uint32_t compressed_size;
    uint64_t skip_size;
    mtpl_status_t status;

    memset(cursor, 0, sizeof(*cursor));
    memset(info, 0, sizeof(*info));
    if (stored_size < MTPL_SFP_CRYPTO_HEADER_SIZE) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    status = mtpl_file_seek(reader->file, entry->payload_offset);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_sfp_read_exact(reader->file, header, sizeof(header));
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (memcmp(
            header,
            MTPL_SFP_CRYPTO_FRAME_MAGIC "\0",
            4u) != 0) {
        return MTPL_STATUS_FORMAT_ERROR;
    }
    if (mtpl_sfp_read_u16(header + 4u) != MTPL_SFP_CRYPTO_FRAME_VERSION) {
        return MTPL_STATUS_UNSUPPORTED_VERSION;
    }
    xor_size = mtpl_sfp_read_u16(header + 16u);
    if (xor_size > MTPL_SFP_CRYPTO_XOR_LIMIT ||
        (uint64_t)MTPL_SFP_CRYPTO_XOR_OFFSET + xor_size > stored_size) {
        return MTPL_STATUS_CORRUPT_DATA;
    }
    stream_init(&cursor->cipher, reader->key.data, reader->key.size);
    cursor->file = reader->file;
    cursor->frame_offset = MTPL_SFP_CRYPTO_HEADER_SIZE;
    cursor->encrypted_end = MTPL_SFP_CRYPTO_XOR_OFFSET + xor_size;
    cursor->disk_remaining = stored_size - MTPL_SFP_CRYPTO_HEADER_SIZE;
    if (xor_size != 0u) {
        size_t header_encrypted_size = xor_size < 2u ? xor_size : 2u;
        stream_encrypt(
            &cursor->cipher,
            header + MTPL_SFP_CRYPTO_XOR_OFFSET,
            header_encrypted_size);
    }

    info->compression_method = (mtpl_compression_method_t)
        mtpl_sfp_read_u16(header + 6u);
    info->original_size = mtpl_sfp_read_u32(header + 8u);
    compressed_size = mtpl_sfp_read_u32(header + 12u);
    data_offset = mtpl_sfp_read_u16(header + 18u);
    if (info->compression_method < MTPL_COMPRESSION_NONE ||
        info->compression_method > MTPL_COMPRESSION_ZLIB) {
        return MTPL_STATUS_UNSUPPORTED;
    }
    if (info->original_size != entry->original_size ||
        data_offset < MTPL_SFP_CRYPTO_HEADER_SIZE ||
        data_offset > stored_size ||
        compressed_size > stored_size - data_offset) {
        return MTPL_STATUS_CORRUPT_DATA;
    }

    skip_size = data_offset - MTPL_SFP_CRYPTO_HEADER_SIZE;
    while (skip_size != 0u) {
        size_t current = skip_size < sizeof(discard)
            ? (size_t)skip_size
            : sizeof(discard);
        status = mtpl_sfp_frame_read_direct(cursor, discard, current);
        if (status != MTPL_STATUS_OK) {
            memset(discard, 0, sizeof(discard));
            return status;
        }
        skip_size -= current;
    }
    memset(discard, 0, sizeof(discard));
    cursor->disk_remaining = compressed_size;
    cursor->input = (uint8_t *)malloc(MTPL_SFP_STREAM_INPUT_SIZE);
    if (cursor->input == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_frame_cursor_fill(
    mtpl_sfp_frame_cursor_t *cursor) {
    size_t current;
    mtpl_status_t status;

    if (cursor->input_offset != cursor->input_size ||
        cursor->disk_remaining == 0u) {
        return MTPL_STATUS_OK;
    }
    current = cursor->disk_remaining < MTPL_SFP_STREAM_INPUT_SIZE
        ? (size_t)cursor->disk_remaining
        : MTPL_SFP_STREAM_INPUT_SIZE;
    status = mtpl_sfp_frame_read_direct(cursor, cursor->input, current);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    cursor->input_offset = 0u;
    cursor->input_size = current;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_frame_cursor_peek(
    mtpl_sfp_frame_cursor_t *cursor,
    const uint8_t **data,
    size_t *size) {
    mtpl_status_t status;

    *data = NULL;
    *size = 0u;
    status = mtpl_sfp_frame_cursor_fill(cursor);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (cursor->input_offset != cursor->input_size) {
        *data = cursor->input + cursor->input_offset;
        *size = cursor->input_size - cursor->input_offset;
    }
    return MTPL_STATUS_OK;
}

static void mtpl_sfp_frame_cursor_consume(
    mtpl_sfp_frame_cursor_t *cursor,
    size_t size) {
    cursor->input_offset += size;
}

static uint64_t mtpl_sfp_frame_cursor_remaining(
    const mtpl_sfp_frame_cursor_t *cursor) {
    return cursor->disk_remaining +
        (uint64_t)(cursor->input_size - cursor->input_offset);
}

static mtpl_status_t mtpl_sfp_frame_cursor_read(
    mtpl_sfp_frame_cursor_t *cursor,
    uint8_t *data,
    size_t size) {
    while (size != 0u) {
        const uint8_t *available_data;
        size_t available_size;
        size_t current;
        mtpl_status_t status = mtpl_sfp_frame_cursor_peek(
            cursor,
            &available_data,
            &available_size);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        if (available_size == 0u) {
            return MTPL_STATUS_DECOMPRESSION_ERROR;
        }
        current = size < available_size ? size : available_size;
        memcpy(data, available_data, current);
        mtpl_sfp_frame_cursor_consume(cursor, current);
        data += current;
        size -= current;
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_frame_cursor_read_byte(
    mtpl_sfp_frame_cursor_t *cursor,
    uint8_t *value) {
    return mtpl_sfp_frame_cursor_read(cursor, value, 1u);
}

static mtpl_status_t mtpl_sfp_stream_snappy(
    mtpl_sfp_frame_cursor_t *cursor,
    mtpl_sfp_stream_output_t *output) {
    uint32_t declared_size = 0u;
    unsigned int shift;
    bool terminated = false;

    for (shift = 0u; shift < 35u; shift += 7u) {
        uint8_t value;
        mtpl_status_t status = mtpl_sfp_frame_cursor_read_byte(cursor, &value);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        if (shift == 28u && (value & 0xf0u) != 0u) {
            return MTPL_STATUS_DECOMPRESSION_ERROR;
        }
        declared_size |= (uint32_t)(value & 0x7fu) << shift;
        if ((value & 0x80u) == 0u) {
            terminated = true;
            break;
        }
    }
    if (!terminated || declared_size != output->expected) {
        return MTPL_STATUS_DECOMPRESSION_ERROR;
    }

    while (mtpl_sfp_frame_cursor_remaining(cursor) != 0u) {
        uint8_t tag;
        uint32_t length;
        mtpl_status_t status = mtpl_sfp_frame_cursor_read_byte(cursor, &tag);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        length = (uint32_t)(tag >> 2u) + 1u;
        if ((tag & 3u) == 0u) {
            if (length > 60u) {
                uint32_t encoded_length = 0u;
                uint32_t extra_size = length - 60u;
                uint32_t index;
                for (index = 0u; index < extra_size; ++index) {
                    uint8_t value;
                    status = mtpl_sfp_frame_cursor_read_byte(cursor, &value);
                    if (status != MTPL_STATUS_OK) {
                        return status;
                    }
                    encoded_length |= (uint32_t)value << (index * 8u);
                }
                if (encoded_length == UINT32_MAX) {
                    return MTPL_STATUS_DECOMPRESSION_ERROR;
                }
                length = encoded_length + 1u;
            }
            if ((uint64_t)length > mtpl_sfp_frame_cursor_remaining(cursor)) {
                return MTPL_STATUS_DECOMPRESSION_ERROR;
            }
            while (length != 0u) {
                const uint8_t *data;
                size_t size;
                size_t current;
                status = mtpl_sfp_frame_cursor_peek(cursor, &data, &size);
                if (status != MTPL_STATUS_OK) {
                    return status;
                }
                if (size == 0u) {
                    return MTPL_STATUS_DECOMPRESSION_ERROR;
                }
                current = length < size ? length : size;
                status = mtpl_sfp_stream_output_write(output, data, current);
                if (status != MTPL_STATUS_OK) {
                    return status;
                }
                mtpl_sfp_frame_cursor_consume(cursor, current);
                length -= (uint32_t)current;
            }
        } else {
            uint32_t offset;
            uint8_t offset_bytes[4] = {0u, 0u, 0u, 0u};
            size_t offset_size;

            if ((tag & 3u) == 1u) {
                length = ((length - 1u) & 7u) + 4u;
                offset_size = 1u;
            } else if ((tag & 3u) == 2u) {
                offset_size = 2u;
            } else {
                offset_size = 4u;
            }
            status = mtpl_sfp_frame_cursor_read(
                cursor,
                offset_bytes,
                offset_size);
            if (status != MTPL_STATUS_OK) {
                return status;
            }
            if ((tag & 3u) == 1u) {
                offset = ((uint32_t)(tag & 0xe0u) << 3u) |
                    offset_bytes[0];
            } else {
                offset = mtpl_sfp_read_u32(offset_bytes);
                if (offset_size == 2u) {
                    offset &= 0xffffu;
                }
            }
            status = mtpl_sfp_stream_output_copy(
                output,
                offset,
                length);
            if (status != MTPL_STATUS_OK) {
                return status;
            }
        }
    }
    return mtpl_sfp_stream_output_finish(output);
}

static mtpl_status_t mtpl_sfp_stream_uncompressed(
    mtpl_sfp_frame_cursor_t *cursor,
    mtpl_sfp_stream_output_t *output) {
    if (mtpl_sfp_frame_cursor_remaining(cursor) != output->expected) {
        return MTPL_STATUS_DECOMPRESSION_ERROR;
    }
    while (mtpl_sfp_frame_cursor_remaining(cursor) != 0u) {
        const uint8_t *data;
        size_t size;
        mtpl_status_t status = mtpl_sfp_frame_cursor_peek(
            cursor,
            &data,
            &size);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        status = mtpl_sfp_stream_output_write(output, data, size);
        if (status != MTPL_STATUS_OK) {
            return status;
        }
        mtpl_sfp_frame_cursor_consume(cursor, size);
    }
    return mtpl_sfp_stream_output_finish(output);
}

static mtpl_status_t mtpl_sfp_stream_zlib(
    mtpl_sfp_frame_cursor_t *cursor,
    mtpl_sfp_stream_output_t *output) {
    z_stream stream;
    int zlib_status;
    mtpl_status_t status = MTPL_STATUS_OK;
    bool finished = false;

    memset(&stream, 0, sizeof(stream));
    zlib_status = inflateInit(&stream);
    if (zlib_status != Z_OK) {
        return zlib_status == Z_MEM_ERROR
            ? MTPL_STATUS_OUT_OF_MEMORY
            : MTPL_STATUS_DECOMPRESSION_ERROR;
    }
    while (status == MTPL_STATUS_OK && !finished) {
        const uint8_t *data;
        size_t size;
        uInt before_input;
        size_t produced;

        status = mtpl_sfp_frame_cursor_peek(cursor, &data, &size);
        if (status != MTPL_STATUS_OK) {
            break;
        }
        if (size == 0u) {
            status = MTPL_STATUS_DECOMPRESSION_ERROR;
            break;
        }
        stream.next_in = (Bytef *)data;
        stream.avail_in = (uInt)size;
        stream.next_out = output->buffer;
        stream.avail_out = (uInt)output->buffer_capacity;
        before_input = stream.avail_in;
        zlib_status = inflate(&stream, Z_NO_FLUSH);
        mtpl_sfp_frame_cursor_consume(
            cursor,
            (size_t)(before_input - stream.avail_in));
        produced = output->buffer_capacity - stream.avail_out;
        if ((uint64_t)produced > output->expected - output->produced) {
            status = MTPL_STATUS_DECOMPRESSION_ERROR;
            break;
        }
        output->buffer_size = produced;
        output->produced += produced;
        status = mtpl_sfp_stream_output_flush(output);
        if (status != MTPL_STATUS_OK) {
            break;
        }
        if (zlib_status == Z_STREAM_END) {
            finished = true;
        } else if (zlib_status != Z_OK ||
            (before_input == stream.avail_in && produced == 0u)) {
            status = zlib_status == Z_MEM_ERROR
                ? MTPL_STATUS_OUT_OF_MEMORY
                : MTPL_STATUS_DECOMPRESSION_ERROR;
        }
    }
    inflateEnd(&stream);
    if (status == MTPL_STATUS_OK &&
        (!finished || output->produced != output->expected)) {
        status = MTPL_STATUS_DECOMPRESSION_ERROR;
    }
    return status;
}

static mtpl_status_t mtpl_sfp_stream_crypto_error(
    mtpl_status_t status) {
    if (status == MTPL_STATUS_CORRUPT_DATA ||
        status == MTPL_STATUS_FORMAT_ERROR ||
        status == MTPL_STATUS_DECOMPRESSION_ERROR ||
        status == MTPL_STATUS_COMPRESSION_ERROR) {
        return MTPL_STATUS_CRYPTO_ERROR;
    }
    return status;
}

static mtpl_status_t mtpl_sfp_stream_plain_entry(
    mtpl_sfp_reader_t *reader,
    const mtpl_sfp_entry_t *entry,
    size_t chunk_size,
    mtpl_sfp_internal_read_chunk_callback_t callback,
    void *user_data) {
    mtpl_sfp_stream_output_t output;
    uint64_t remaining = entry->original_size;
    mtpl_status_t status = mtpl_sfp_stream_output_init(
        &output,
        entry->original_size,
        chunk_size,
        0u,
        callback,
        user_data);

    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_file_seek(reader->file, entry->payload_offset);
    while (status == MTPL_STATUS_OK && remaining != 0u) {
        size_t current = remaining < output.buffer_capacity
            ? (size_t)remaining
            : output.buffer_capacity;
        status = mtpl_sfp_read_exact(reader->file, output.buffer, current);
        if (status == MTPL_STATUS_OK) {
            output.buffer_size = current;
            output.produced += current;
            remaining -= current;
            status = mtpl_sfp_stream_output_flush(&output);
        }
    }
    mtpl_sfp_stream_output_release(&output);
    return status;
}

mtpl_status_t mtpl_sfp_reader_read_file_chunks_internal(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    size_t chunk_size,
    mtpl_sfp_internal_read_chunk_callback_t callback,
    void *user_data) {
    mtpl_sfp_entry_t *entry;
    mtpl_sfp_frame_cursor_t cursor;
    mtpl_sfp_frame_info_t frame_info;
    mtpl_sfp_stream_output_t output;
    size_t history_capacity = 0u;
    mtpl_status_t status;

    if (chunk_size == 0u || callback == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_sfp_lookup_normalized(reader, package_path, &entry);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (entry->encrypted_size == 0u) {
        return mtpl_sfp_stream_plain_entry(
            reader,
            entry,
            chunk_size,
            callback,
            user_data);
    }
    if (reader->key.data == NULL) {
        return MTPL_STATUS_KEY_REQUIRED;
    }
    status = mtpl_sfp_frame_cursor_init(reader, entry, &cursor, &frame_info);
    if (status != MTPL_STATUS_OK) {
        mtpl_sfp_frame_cursor_release(&cursor);
        return mtpl_sfp_stream_crypto_error(status);
    }
    if (frame_info.compression_method == MTPL_COMPRESSION_SNAPPY) {
        history_capacity = MTPL_SFP_STREAM_SNAPPY_HISTORY_SIZE;
    }
    status = mtpl_sfp_stream_output_init(
        &output,
        frame_info.original_size,
        chunk_size,
        history_capacity,
        callback,
        user_data);
    if (status == MTPL_STATUS_OK) {
        if (frame_info.compression_method == MTPL_COMPRESSION_NONE) {
            status = mtpl_sfp_stream_uncompressed(&cursor, &output);
        } else if (frame_info.compression_method == MTPL_COMPRESSION_SNAPPY) {
            status = mtpl_sfp_stream_snappy(&cursor, &output);
        } else {
            status = mtpl_sfp_stream_zlib(&cursor, &output);
        }
    }
    if (status != MTPL_STATUS_OK &&
        output.callback_status == MTPL_STATUS_OK) {
        status = mtpl_sfp_stream_crypto_error(status);
    }
    mtpl_sfp_stream_output_release(&output);
    mtpl_sfp_frame_cursor_release(&cursor);
    return status;
}

mtpl_status_t mtpl_sfp_reader_read_file(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    mtpl_buffer_t *buffer) {
    mtpl_sfp_entry_t *entry;
    mtpl_status_t status;
    uint64_t stored_size;
    uint8_t *stored = NULL;

    if (buffer == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    mtpl_sfp_zero_buffer(buffer);
    status = mtpl_sfp_lookup_normalized(reader, package_path, &entry);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (entry->encrypted_size != 0u && reader->key.data == NULL) {
        return MTPL_STATUS_KEY_REQUIRED;
    }
    stored_size = mtpl_sfp_entry_stored_size(entry);
    if (stored_size != 0u) {
        stored = (uint8_t *)malloc((size_t)stored_size);
        if (stored == NULL) {
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        status = mtpl_file_seek(reader->file, entry->payload_offset);
        if (status == MTPL_STATUS_OK) {
            status = mtpl_sfp_read_exact(
                reader->file,
                stored,
                (size_t)stored_size);
        }
        if (status != MTPL_STATUS_OK) {
            free(stored);
            return status;
        }
    }
    if (entry->encrypted_size != 0u) {
        mtpl_buffer_view_t encrypted = { stored, (size_t)stored_size };
        if (stored_size < 12u ||
            mtpl_sfp_read_u32(stored + 8u) != entry->original_size) {
            free(stored);
            return MTPL_STATUS_CORRUPT_DATA;
        }
        status = mtpl_crypto_decrypt_frame(
            &reader->key,
            encrypted,
            entry->original_size,
            buffer);
        free(stored);
        if (status != MTPL_STATUS_OK) {
            if (status == MTPL_STATUS_CORRUPT_DATA ||
                status == MTPL_STATUS_FORMAT_ERROR ||
                status == MTPL_STATUS_DECOMPRESSION_ERROR ||
                status == MTPL_STATUS_COMPRESSION_ERROR) {
                return MTPL_STATUS_CRYPTO_ERROR;
            }
            return status;
        }
        if (buffer->size != entry->original_size) {
            mtpl_sfp_release_buffer(buffer);
            return MTPL_STATUS_CORRUPT_DATA;
        }
        return MTPL_STATUS_OK;
    }
    buffer->data = stored;
    buffer->size = entry->original_size;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_sfp_reader_read_text(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    mtpl_buffer_t *buffer) {
    uint8_t *terminated;
    mtpl_status_t status;

    if (buffer == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_sfp_reader_read_file(reader, package_path, buffer);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (buffer->size == SIZE_MAX) {
        mtpl_sfp_release_buffer(buffer);
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    terminated = (uint8_t *)realloc(buffer->data, buffer->size + 1u);
    if (terminated == NULL) {
        mtpl_sfp_release_buffer(buffer);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    terminated[buffer->size] = 0u;
    buffer->data = terminated;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_sfp_reader_get_file_size(
    mtpl_sfp_reader_t *reader,
    const char *package_path,
    uint64_t *size) {
    mtpl_sfp_entry_t *entry;
    mtpl_status_t status;

    if (size == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *size = 0u;
    status = mtpl_sfp_lookup_normalized(reader, package_path, &entry);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    *size = entry->original_size;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_list_add_file(
    mtpl_sfp_list_t *list,
    const char *name,
    uint64_t size) {
    mtpl_sfp_list_file_t *files;
    char *copy;

    if (list->file_count == SIZE_MAX / sizeof(*files)) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    copy = mtpl_sfp_strdup(name);
    if (copy == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    files = (mtpl_sfp_list_file_t *)realloc(
        list->files,
        (list->file_count + 1u) * sizeof(*files));
    if (files == NULL) {
        free(copy);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    list->files = files;
    list->files[list->file_count].name = copy;
    list->files[list->file_count].size = size;
    ++list->file_count;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_list_add_directory(
    mtpl_sfp_list_t *list,
    const char *name,
    size_t name_size) {
    char **directories;
    char *copy;

    if (list->directory_count != 0u) {
        const char *previous = list->directories[list->directory_count - 1u];
        if (strlen(previous) == name_size &&
            memcmp(previous, name, name_size) == 0) {
            return MTPL_STATUS_OK;
        }
    }
    if (list->directory_count == SIZE_MAX / sizeof(*directories)) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    copy = (char *)malloc(name_size + 1u);
    if (copy == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(copy, name, name_size);
    copy[name_size] = '\0';
    directories = (char **)realloc(
        list->directories,
        (list->directory_count + 1u) * sizeof(*directories));
    if (directories == NULL) {
        free(copy);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    list->directories = directories;
    list->directories[list->directory_count++] = copy;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_sfp_reader_list(
    mtpl_sfp_reader_t *reader,
    const char *package_directory,
    mtpl_sfp_list_t **list) {
    mtpl_sfp_list_t *created;
    char *directory = NULL;
    size_t directory_size;
    size_t index;
    mtpl_status_t status;

    if (list == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *list = NULL;
    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_sfp_normalize_package_path(
        package_directory,
        true,
        true,
        &directory);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    directory_size = strlen(directory);
    created = (mtpl_sfp_list_t *)calloc(1u, sizeof(*created));
    if (created == NULL) {
        free(directory);
        return MTPL_STATUS_OUT_OF_MEMORY;
    }

    for (index = 0u; index < reader->entry_count; ++index) {
        const mtpl_sfp_entry_t *entry = &reader->entries[index];
        const char *relative;
        const char *separator;

        if (directory_size == 0u) {
            relative = entry->path;
        } else {
            if (strncmp(entry->path, directory, directory_size) != 0 ||
                entry->path[directory_size] != '/') {
                continue;
            }
            relative = entry->path + directory_size + 1u;
        }
        separator = strchr(relative, '/');
        if (separator == NULL) {
            status = mtpl_sfp_list_add_file(
                created,
                relative,
                entry->original_size);
        } else {
            status = mtpl_sfp_list_add_directory(
                created,
                relative,
                (size_t)(separator - relative));
        }
        if (status != MTPL_STATUS_OK) {
            mtpl_sfp_list_release(created);
            free(directory);
            return status;
        }
    }
    free(directory);
    *list = created;
    return MTPL_STATUS_OK;
}

size_t mtpl_sfp_list_file_count(const mtpl_sfp_list_t *list) {
    return list == NULL ? 0u : list->file_count;
}

mtpl_status_t mtpl_sfp_list_get_file(
    const mtpl_sfp_list_t *list,
    size_t index,
    const char **name,
    uint64_t *size) {
    if (name == NULL || size == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *name = NULL;
    *size = 0u;
    if (list == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (index >= list->file_count) {
        return MTPL_STATUS_OUT_OF_RANGE;
    }
    *name = list->files[index].name;
    *size = list->files[index].size;
    return MTPL_STATUS_OK;
}

size_t mtpl_sfp_list_directory_count(const mtpl_sfp_list_t *list) {
    return list == NULL ? 0u : list->directory_count;
}

mtpl_status_t mtpl_sfp_list_get_directory(
    const mtpl_sfp_list_t *list,
    size_t index,
    const char **name) {
    if (name == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *name = NULL;
    if (list == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (index >= list->directory_count) {
        return MTPL_STATUS_OUT_OF_RANGE;
    }
    *name = list->directories[index];
    return MTPL_STATUS_OK;
}

void mtpl_sfp_list_release(mtpl_sfp_list_t *list) {
    size_t index;

    if (list == NULL) {
        return;
    }
    for (index = 0u; index < list->file_count; ++index) {
        free(list->files[index].name);
    }
    for (index = 0u; index < list->directory_count; ++index) {
        free(list->directories[index]);
    }
    free(list->files);
    free(list->directories);
    free(list);
}

static mtpl_status_t mtpl_sfp_string_reserve(
    mtpl_sfp_string_t *string,
    size_t additional) {
    size_t required;
    size_t capacity;
    uint8_t *data;

    if (additional > SIZE_MAX - string->size - 1u) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    required = string->size + additional + 1u;
    if (required <= string->capacity) {
        return MTPL_STATUS_OK;
    }
    capacity = string->capacity == 0u ? 64u : string->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    data = (uint8_t *)realloc(string->data, capacity);
    if (data == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    string->data = data;
    string->capacity = capacity;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_string_append(
    mtpl_sfp_string_t *string,
    const void *data,
    size_t size) {
    mtpl_status_t status = mtpl_sfp_string_reserve(string, size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (size != 0u) {
        memcpy(string->data + string->size, data, size);
    }
    string->size += size;
    string->data[string->size] = 0u;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_string_append_json_string(
    mtpl_sfp_string_t *string,
    const char *value) {
    static const char hex[] = "0123456789abcdef";
    const uint8_t quote = '"';
    size_t index;
    mtpl_status_t status;

    status = mtpl_sfp_string_append(string, &quote, 1u);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    for (index = 0u; value[index] != '\0'; ++index) {
        const uint8_t byte = (uint8_t)value[index];
        const char *escape = NULL;
        char unicode_escape[6];

        switch (byte) {
            case '"': escape = "\\\""; break;
            case '\\': escape = "\\\\"; break;
            case '\b': escape = "\\b"; break;
            case '\f': escape = "\\f"; break;
            case '\n': escape = "\\n"; break;
            case '\r': escape = "\\r"; break;
            case '\t': escape = "\\t"; break;
            default: break;
        }
        if (escape != NULL) {
            status = mtpl_sfp_string_append(string, escape, 2u);
        } else if (byte < 0x20u) {
            unicode_escape[0] = '\\';
            unicode_escape[1] = 'u';
            unicode_escape[2] = '0';
            unicode_escape[3] = '0';
            unicode_escape[4] = hex[byte >> 4u];
            unicode_escape[5] = hex[byte & 0x0fu];
            status = mtpl_sfp_string_append(string, unicode_escape, 6u);
        } else {
            status = mtpl_sfp_string_append(string, &byte, 1u);
        }
        if (status != MTPL_STATUS_OK) {
            return status;
        }
    }
    return mtpl_sfp_string_append(string, &quote, 1u);
}

mtpl_status_t mtpl_sfp_reader_list_json(
    mtpl_sfp_reader_t *reader,
    mtpl_buffer_t *json) {
    mtpl_sfp_string_t result = { 0 };
    size_t index;
    mtpl_status_t status;

    if (json == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    mtpl_sfp_zero_buffer(json);
    if (reader == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_sfp_string_append(&result, "{", 1u);
    for (index = 0u; status == MTPL_STATUS_OK && index < reader->entry_count; ++index) {
        const mtpl_sfp_entry_t *entry = &reader->entries[index];
        char numbers[96];
        int length;

        if (index != 0u) {
            status = mtpl_sfp_string_append(&result, ",", 1u);
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_sfp_string_append_json_string(&result, entry->path);
        }
        if (status == MTPL_STATUS_OK) {
            length = snprintf(
                numbers,
                sizeof(numbers),
                ":[%" PRIu64 ",%" PRIu64 "]",
                entry->payload_offset,
                mtpl_sfp_entry_stored_size(entry));
            if (length < 0 || (size_t)length >= sizeof(numbers)) {
                status = MTPL_STATUS_INTERNAL_ERROR;
            } else {
                status = mtpl_sfp_string_append(
                    &result,
                    numbers,
                    (size_t)length);
            }
        }
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_string_append(&result, "}", 1u);
    }
    if (status != MTPL_STATUS_OK) {
        free(result.data);
        return status;
    }
    json->data = result.data;
    json->size = result.size;
    return MTPL_STATUS_OK;
}

#ifdef _WIN32
static wchar_t *mtpl_sfp_utf8_to_wide(const char *value) {
    return mtpl_path_to_wide(value);
}

static char *mtpl_sfp_wide_to_utf8(const wchar_t *value) {
    int required;
    char *utf8;

    required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        -1,
        NULL,
        0,
        NULL,
        NULL);
    if (required <= 0) {
        return NULL;
    }
    utf8 = (char *)malloc((size_t)required);
    if (utf8 == NULL) {
        return NULL;
    }
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value,
            -1,
            utf8,
            required,
            NULL,
            NULL) == 0) {
        free(utf8);
        return NULL;
    }
    return utf8;
}
#endif

static mtpl_status_t mtpl_sfp_path_info(
    const char *path,
    bool *exists,
    bool *is_directory,
    bool *is_link,
    uint64_t *size) {
    if (path == NULL || exists == NULL || is_directory == NULL ||
        is_link == NULL || size == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *exists = false;
    *is_directory = false;
    *is_link = false;
    *size = 0u;
#ifdef _WIN32
    {
        WIN32_FILE_ATTRIBUTE_DATA attributes;
        wchar_t *wide = mtpl_sfp_utf8_to_wide(path);
        DWORD error;
        if (wide == NULL) {
            return MTPL_STATUS_PATH_ERROR;
        }
        if (!GetFileAttributesExW(wide, GetFileExInfoStandard, &attributes)) {
            error = GetLastError();
            free(wide);
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
                return MTPL_STATUS_OK;
            }
            return MTPL_STATUS_IO_ERROR;
        }
        free(wide);
        *exists = true;
        *is_directory =
            (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        *is_link =
            (attributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        *size = ((uint64_t)attributes.nFileSizeHigh << 32u) |
            attributes.nFileSizeLow;
    }
#else
    {
        struct stat info;
        if (lstat(path, &info) != 0) {
            if (errno == ENOENT || errno == ENOTDIR) {
                return MTPL_STATUS_OK;
            }
            return MTPL_STATUS_IO_ERROR;
        }
        *exists = true;
        *is_directory = S_ISDIR(info.st_mode);
        *is_link = S_ISLNK(info.st_mode);
        if (info.st_size < 0) {
            return MTPL_STATUS_IO_ERROR;
        }
        *size = (uint64_t)info.st_size;
    }
#endif
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_mkdir_one(const char *path) {
#ifdef _WIN32
    wchar_t *wide = mtpl_sfp_utf8_to_wide(path);
    int result;
    if (wide == NULL) {
        return MTPL_STATUS_PATH_ERROR;
    }
    result = _wmkdir(wide);
    free(wide);
#else
    int result = mkdir(path, 0777);
#endif
    if (result == 0 || errno == EEXIST) {
        return MTPL_STATUS_OK;
    }
    return MTPL_STATUS_IO_ERROR;
}

static void mtpl_sfp_remove_utf8(const char *path) {
#ifdef _WIN32
    wchar_t *wide = mtpl_sfp_utf8_to_wide(path);
    if (wide != NULL) {
        _wremove(wide);
        free(wide);
    }
#else
    remove(path);
#endif
}

static mtpl_status_t mtpl_sfp_rename_replace(
    const char *source,
    const char *destination) {
#ifdef _WIN32
    wchar_t *wide_source = mtpl_sfp_utf8_to_wide(source);
    wchar_t *wide_destination = mtpl_sfp_utf8_to_wide(destination);
    bool moved;
    if (wide_source == NULL || wide_destination == NULL) {
        free(wide_source);
        free(wide_destination);
        return MTPL_STATUS_PATH_ERROR;
    }
    moved = MoveFileExW(
        wide_source,
        wide_destination,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    free(wide_source);
    free(wide_destination);
    return moved ? MTPL_STATUS_OK : MTPL_STATUS_IO_ERROR;
#else
    return rename(source, destination) == 0
        ? MTPL_STATUS_OK
        : MTPL_STATUS_IO_ERROR;
#endif
}

static bool mtpl_sfp_is_file_separator(char value) {
#ifdef _WIN32
    return value == '/' || value == '\\';
#else
    return value == '/';
#endif
}

static char *mtpl_sfp_join_path(const char *left, const char *right) {
    size_t left_size;
    size_t right_size;
    bool separator;
    char *joined;

    if (left == NULL || right == NULL) {
        return NULL;
    }
    left_size = strlen(left);
    right_size = strlen(right);
    separator = left_size != 0u &&
        !mtpl_sfp_is_file_separator(left[left_size - 1u]);
    if (left_size > SIZE_MAX - right_size - (separator ? 2u : 1u)) {
        return NULL;
    }
    joined = (char *)malloc(
        left_size + right_size + (separator ? 2u : 1u));
    if (joined == NULL) {
        return NULL;
    }
    memcpy(joined, left, left_size);
    if (separator) {
        joined[left_size++] = '/';
    }
    memcpy(joined + left_size, right, right_size + 1u);
    return joined;
}

static mtpl_status_t mtpl_sfp_get_regular_file_size(
    const char *path,
    uint32_t *size) {
    bool exists;
    bool is_directory;
    bool is_link;
    uint64_t file_size;
    mtpl_status_t status;

    status = mtpl_sfp_path_info(
        path,
        &exists,
        &is_directory,
        &is_link,
        &file_size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (!exists) {
        return MTPL_STATUS_FILE_NOT_FOUND;
    }
    if (is_link) {
        return MTPL_STATUS_PATH_TRAVERSAL;
    }
    if (is_directory) {
        return MTPL_STATUS_PATH_ERROR;
    }
    if (file_size > UINT32_MAX) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    *size = (uint32_t)file_size;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_builder_reserve(mtpl_sfp_builder_t *builder) {
    size_t capacity;
    mtpl_sfp_builder_entry_t *entries;

    if (builder->entry_count < builder->entry_capacity) {
        return MTPL_STATUS_OK;
    }
    if (builder->entry_capacity >= MTPL_SFP_MAX_FILE_COUNT) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    capacity = builder->entry_capacity == 0u
        ? 16u
        : builder->entry_capacity * 2u;
    if (capacity > MTPL_SFP_MAX_FILE_COUNT) {
        capacity = MTPL_SFP_MAX_FILE_COUNT;
    }
    entries = (mtpl_sfp_builder_entry_t *)realloc(
        builder->entries,
        capacity * sizeof(*entries));
    if (entries == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memset(
        entries + builder->entry_capacity,
        0,
        (capacity - builder->entry_capacity) * sizeof(*entries));
    builder->entries = entries;
    builder->entry_capacity = capacity;
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_builder_prepare_entry(
    mtpl_sfp_builder_t *builder,
    const char *package_path,
    mtpl_storage_mode_t mode,
    mtpl_sfp_builder_entry_t **entry) {
    char *normalized = NULL;
    uint64_t id;
    size_t index;
    mtpl_status_t status;

    if (builder == NULL || entry == NULL ||
        (mode != MTPL_STORAGE_PLAIN && mode != MTPL_STORAGE_ENCRYPTED)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *entry = NULL;
    if (mode == MTPL_STORAGE_ENCRYPTED && builder->key.data == NULL) {
        return MTPL_STATUS_KEY_REQUIRED;
    }
    status = mtpl_sfp_normalize_package_path(
        package_path,
        false,
        false,
        &normalized);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (strlen(normalized) > UINT16_MAX) {
        free(normalized);
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    id = mtpl_sfp_path_id(normalized);
    for (index = 0u; index < builder->entry_count; ++index) {
        if (builder->entries[index].id == id ||
            strcmp(builder->entries[index].package_path, normalized) == 0 ||
            mtpl_sfp_paths_collide_on_windows(
                builder->entries[index].package_path,
                normalized)) {
            free(normalized);
            return MTPL_STATUS_ALREADY_EXISTS;
        }
    }
    status = mtpl_sfp_builder_reserve(builder);
    if (status != MTPL_STATUS_OK) {
        free(normalized);
        return status;
    }
    *entry = &builder->entries[builder->entry_count];
    (*entry)->package_path = normalized;
    (*entry)->id = id;
    (*entry)->mode = mode;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_sfp_builder_create(
    const mtpl_crypto_options_t *crypto,
    mtpl_sfp_builder_t **builder) {
    mtpl_sfp_builder_t *created;
    mtpl_status_t status = MTPL_STATUS_OK;

    if (builder == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *builder = NULL;
    created = (mtpl_sfp_builder_t *)calloc(1u, sizeof(*created));
    if (created == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    if (crypto != NULL) {
        status = mtpl_crypto_derive_key(crypto, &created->key);
    }
    if (status != MTPL_STATUS_OK) {
        free(created);
        return status;
    }
    *builder = created;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_sfp_builder_add_file(
    mtpl_sfp_builder_t *builder,
    const char *package_path,
    const char *source_path,
    mtpl_storage_mode_t mode) {
    mtpl_sfp_builder_entry_t *entry;
    uint32_t size;
    mtpl_status_t status;

    if (source_path == NULL || source_path[0] == '\0') {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_path_is_valid_utf8(source_path)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    status = mtpl_sfp_get_regular_file_size(source_path, &size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    status = mtpl_sfp_builder_prepare_entry(
        builder,
        package_path,
        mode,
        &entry);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    entry->source_path = mtpl_sfp_strdup(source_path);
    if (entry->source_path == NULL) {
        free(entry->package_path);
        memset(entry, 0, sizeof(*entry));
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    entry->source_is_file = true;
    entry->size = size;
    ++builder->entry_count;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_sfp_builder_add_data(
    mtpl_sfp_builder_t *builder,
    const char *package_path,
    mtpl_buffer_view_t data,
    mtpl_storage_mode_t mode) {
    mtpl_sfp_builder_entry_t *entry;
    mtpl_status_t status;

    if ((data.data == NULL && data.size != 0u) || data.size > UINT32_MAX) {
        return data.size > UINT32_MAX
            ? MTPL_STATUS_LIMIT_EXCEEDED
            : MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_sfp_builder_prepare_entry(
        builder,
        package_path,
        mode,
        &entry);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (data.size != 0u) {
        entry->data = (uint8_t *)malloc(data.size);
        if (entry->data == NULL) {
            free(entry->package_path);
            memset(entry, 0, sizeof(*entry));
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        memcpy(entry->data, data.data, data.size);
    }
    entry->size = (uint32_t)data.size;
    ++builder->entry_count;
    return MTPL_STATUS_OK;
}

void mtpl_sfp_builder_destroy(mtpl_sfp_builder_t *builder) {
    size_t index;

    if (builder == NULL) {
        return;
    }
    for (index = 0u; index < builder->entry_count; ++index) {
        free(builder->entries[index].package_path);
        free(builder->entries[index].source_path);
        free(builder->entries[index].data);
    }
    free(builder->entries);
    mtpl_crypto_key_release(&builder->key);
    free(builder);
}

static mtpl_status_t mtpl_sfp_load_builder_entry(
    const mtpl_sfp_builder_entry_t *entry,
    mtpl_buffer_t *buffer) {
    FILE *file;
    uint64_t current_size;
    mtpl_status_t status;

    mtpl_sfp_zero_buffer(buffer);
    if (!entry->source_is_file) {
        buffer->data = entry->data;
        buffer->size = entry->size;
        return MTPL_STATUS_OK;
    }
    file = mtpl_fopen_utf8(entry->source_path, "rb");
    if (file == NULL) {
        return errno == ENOENT
            ? MTPL_STATUS_FILE_NOT_FOUND
            : MTPL_STATUS_IO_ERROR;
    }
    status = mtpl_file_get_size(file, &current_size);
    if (status == MTPL_STATUS_OK && current_size != entry->size) {
        status = MTPL_STATUS_IO_ERROR;
    }
    if (status == MTPL_STATUS_OK && entry->size != 0u) {
        buffer->data = (uint8_t *)malloc(entry->size);
        if (buffer->data == NULL) {
            status = MTPL_STATUS_OUT_OF_MEMORY;
        }
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_file_seek(file, 0u);
    }
    if (status == MTPL_STATUS_OK && entry->size != 0u) {
        status = mtpl_sfp_read_exact(file, buffer->data, entry->size);
    }
    if (fclose(file) != 0 && status == MTPL_STATUS_OK) {
        status = MTPL_STATUS_IO_ERROR;
    }
    if (status != MTPL_STATUS_OK) {
        free(buffer->data);
        mtpl_sfp_zero_buffer(buffer);
        return status;
    }
    buffer->size = entry->size;
    return MTPL_STATUS_OK;
}

static char *mtpl_sfp_make_temporary_path(const char *output_path) {
    unsigned long process_id;
    unsigned int attempt;
    size_t capacity;
    char *temporary;

#ifdef _WIN32
    process_id = (unsigned long)GetCurrentProcessId();
#else
    process_id = (unsigned long)getpid();
#endif
    if (strlen(output_path) > SIZE_MAX - 64u) {
        return NULL;
    }
    capacity = strlen(output_path) + 64u;
    temporary = (char *)malloc(capacity);
    if (temporary == NULL) {
        return NULL;
    }
    for (attempt = 0u; attempt < 1000u; ++attempt) {
        bool exists;
        bool is_directory;
        bool is_link;
        uint64_t size;
        mtpl_status_t status;

        if (snprintf(
                temporary,
                capacity,
                "%s.mtpl-tmp-%lu-%u",
                output_path,
                process_id,
                attempt) < 0) {
            free(temporary);
            return NULL;
        }
        status = mtpl_sfp_path_info(
            temporary,
            &exists,
            &is_directory,
            &is_link,
            &size);
        if (status == MTPL_STATUS_OK && !exists) {
            return temporary;
        }
        if (status != MTPL_STATUS_OK) {
            free(temporary);
            return NULL;
        }
    }
    free(temporary);
    return NULL;
}

mtpl_status_t mtpl_sfp_builder_write_with_control(
    mtpl_sfp_builder_t *builder,
    const char *output_path,
    mtpl_sfp_internal_progress_callback_t progress_callback,
    mtpl_sfp_internal_cancel_callback_t cancel_callback,
    void *user_data) {
    uint8_t *header = NULL;
    uint64_t header_size;
    char *temporary_path = NULL;
    FILE *output = NULL;
    size_t index;
    mtpl_status_t status = MTPL_STATUS_OK;

    if (builder == NULL || output_path == NULL || output_path[0] == '\0') {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_utf8_is_valid(
            (const uint8_t *)output_path,
            strlen(output_path))) {
        return MTPL_STATUS_PATH_ERROR;
    }
    header_size = MTPL_SFP_FIXED_HEADER_SIZE +
        (uint64_t)builder->entry_count * MTPL_SFP_INDEX_ENTRY_SIZE;
    if (header_size > SIZE_MAX) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    header = (uint8_t *)calloc((size_t)header_size, 1u);
    if (header == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(header, MTPL_SFP_MAGIC "\0", MTPL_SFP_MAGIC_SIZE);
    mtpl_sfp_write_u32(header + 4u, MTPL_SFP_VERSION);
    mtpl_sfp_write_u32(header + 8u, (uint32_t)builder->entry_count);
    mtpl_sfp_write_u32(header + 12u, 1u);

    temporary_path = mtpl_sfp_make_temporary_path(output_path);
    if (temporary_path == NULL) {
        free(header);
        return MTPL_STATUS_IO_ERROR;
    }
    output = mtpl_fopen_utf8(temporary_path, "wb");
    if (output == NULL) {
        free(temporary_path);
        free(header);
        return MTPL_STATUS_IO_ERROR;
    }
    status = mtpl_sfp_write_exact(output, header, (size_t)header_size);

    if (status == MTPL_STATUS_OK && cancel_callback != NULL &&
        cancel_callback(user_data)) {
        status = MTPL_STATUS_CANCELED;
    }

    for (index = 0u;
        status == MTPL_STATUS_OK && index < builder->entry_count;
        ++index) {
        mtpl_sfp_builder_entry_t *entry = &builder->entries[index];
        mtpl_buffer_t input = { 0 };
        mtpl_buffer_t encrypted = { 0 };
        mtpl_buffer_view_t payload;
        uint8_t path_size_data[2];
        uint8_t *record = header + MTPL_SFP_FIXED_HEADER_SIZE +
            index * MTPL_SFP_INDEX_ENTRY_SIZE;

        if (cancel_callback != NULL && cancel_callback(user_data)) {
            status = MTPL_STATUS_CANCELED;
            break;
        }

        status = mtpl_sfp_load_builder_entry(entry, &input);
        if (status != MTPL_STATUS_OK) {
            break;
        }
        payload.data = input.data;
        payload.size = input.size;
        if (entry->mode == MTPL_STORAGE_ENCRYPTED) {
            status = mtpl_crypto_encrypt_frame(
                &builder->key,
                payload,
                MTPL_COMPRESSION_SNAPPY,
                &encrypted);
            if (status == MTPL_STATUS_OK && encrypted.size > UINT32_MAX) {
                status = MTPL_STATUS_LIMIT_EXCEEDED;
            }
            if (status == MTPL_STATUS_OK) {
                payload.data = encrypted.data;
                payload.size = encrypted.size;
                entry->encrypted_size = (uint32_t)encrypted.size;
            }
        } else {
            entry->encrypted_size = 0u;
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_file_tell(output, &entry->record_offset);
        }
        if (status == MTPL_STATUS_OK) {
            mtpl_sfp_write_u16(
                path_size_data,
                (uint16_t)strlen(entry->package_path));
            status = mtpl_sfp_write_exact(output, path_size_data, 2u);
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_sfp_write_exact(
                output,
                entry->package_path,
                strlen(entry->package_path));
        }
        if (status == MTPL_STATUS_OK) {
            status = mtpl_sfp_write_exact(output, payload.data, payload.size);
        }

        mtpl_sfp_write_u64(record, entry->id);
        mtpl_sfp_write_u64(record + 8u, entry->record_offset);
        mtpl_sfp_write_u32(record + 16u, entry->size);
        mtpl_sfp_write_u32(record + 20u, entry->encrypted_size);
        mtpl_sfp_release_buffer(&encrypted);
        if (entry->source_is_file) {
            mtpl_sfp_release_buffer(&input);
        }
        if (status == MTPL_STATUS_OK && progress_callback != NULL) {
            progress_callback(
                (uint64_t)index + 1u,
                (uint64_t)builder->entry_count,
                user_data);
        }
    }

    if (status == MTPL_STATUS_OK && cancel_callback != NULL &&
        cancel_callback(user_data)) {
        status = MTPL_STATUS_CANCELED;
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_file_seek(output, 0u);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_write_exact(output, header, (size_t)header_size);
    }
    if (status == MTPL_STATUS_OK && fflush(output) != 0) {
        status = MTPL_STATUS_IO_ERROR;
    }
    if (fclose(output) != 0 && status == MTPL_STATUS_OK) {
        status = MTPL_STATUS_IO_ERROR;
    }
    output = NULL;
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_rename_replace(temporary_path, output_path);
    }
    if (status != MTPL_STATUS_OK) {
        mtpl_sfp_remove_utf8(temporary_path);
    }
    free(temporary_path);
    free(header);
    return status;
}

mtpl_status_t mtpl_sfp_builder_write(
    mtpl_sfp_builder_t *builder,
    const char *output_path) {
    return mtpl_sfp_builder_write_with_control(
        builder,
        output_path,
        NULL,
        NULL,
        NULL);
}

static mtpl_status_t mtpl_sfp_ensure_directory(
    const char *path,
    bool reject_link) {
    bool exists;
    bool is_directory;
    bool is_link;
    uint64_t size;
    mtpl_status_t status;
    char *trimmed;
    char *separator;

    if (path == NULL || path[0] == '\0') {
        return MTPL_STATUS_PATH_ERROR;
    }
    trimmed = mtpl_sfp_strdup(path);
    if (trimmed == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    while (strlen(trimmed) > 1u &&
        mtpl_sfp_is_file_separator(trimmed[strlen(trimmed) - 1u])) {
#ifdef _WIN32
        if (strlen(trimmed) == 3u && trimmed[1] == ':') {
            break;
        }
#endif
        trimmed[strlen(trimmed) - 1u] = '\0';
    }
    status = mtpl_sfp_path_info(
        trimmed,
        &exists,
        &is_directory,
        &is_link,
        &size);
    if (status != MTPL_STATUS_OK) {
        free(trimmed);
        return status;
    }
    if (exists) {
        free(trimmed);
        if (reject_link && is_link) {
            return MTPL_STATUS_PATH_TRAVERSAL;
        }
        return is_directory ? MTPL_STATUS_OK : MTPL_STATUS_PATH_ERROR;
    }

    separator = strrchr(trimmed, '/');
#ifdef _WIN32
    {
        char *backslash = strrchr(trimmed, '\\');
        if (backslash != NULL && (separator == NULL || backslash > separator)) {
            separator = backslash;
        }
    }
#endif
    if (separator != NULL && separator != trimmed) {
        char saved = *separator;
        *separator = '\0';
        status = mtpl_sfp_ensure_directory(trimmed, reject_link);
        *separator = saved;
        if (status != MTPL_STATUS_OK) {
            free(trimmed);
            return status;
        }
    }
    status = mtpl_sfp_mkdir_one(trimmed);
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_path_info(
            trimmed,
            &exists,
            &is_directory,
            &is_link,
            &size);
        if (status == MTPL_STATUS_OK &&
            (!exists || !is_directory || (reject_link && is_link))) {
            status = is_link
                ? MTPL_STATUS_PATH_TRAVERSAL
                : MTPL_STATUS_PATH_ERROR;
        }
    }
    free(trimmed);
    return status;
}

static mtpl_status_t mtpl_sfp_prepare_output_file(
    const char *root,
    const char *package_path,
    char **output_path) {
    char *current;
    const char *segment = package_path;
    const char *separator;
    mtpl_status_t status;

    if (output_path == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *output_path = NULL;
#ifdef _WIN32
    if (!mtpl_sfp_path_is_windows_output_compatible(package_path)) {
        return MTPL_STATUS_PATH_ERROR;
    }
#endif
    status = mtpl_sfp_ensure_directory(root, false);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    current = mtpl_sfp_strdup(root);
    if (current == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    while ((separator = strchr(segment, '/')) != NULL) {
        size_t segment_size = (size_t)(separator - segment);
        char *name = (char *)malloc(segment_size + 1u);
        char *next;

        if (name == NULL) {
            free(current);
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        memcpy(name, segment, segment_size);
        name[segment_size] = '\0';
        next = mtpl_sfp_join_path(current, name);
        free(name);
        free(current);
        if (next == NULL) {
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        current = next;
        status = mtpl_sfp_ensure_directory(current, true);
        if (status != MTPL_STATUS_OK) {
            free(current);
            return status;
        }
        segment = separator + 1u;
    }
    *output_path = mtpl_sfp_join_path(current, segment);
    free(current);
    if (*output_path == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
    {
        bool exists;
        bool is_directory;
        bool is_link;
        uint64_t size;
        status = mtpl_sfp_path_info(
            *output_path,
            &exists,
            &is_directory,
            &is_link,
            &size);
        if (status != MTPL_STATUS_OK || (exists && (is_directory || is_link))) {
            free(*output_path);
            *output_path = NULL;
            if (status != MTPL_STATUS_OK) {
                return status;
            }
            return is_link
                ? MTPL_STATUS_PATH_TRAVERSAL
                : MTPL_STATUS_PATH_ERROR;
        }
    }
    return MTPL_STATUS_OK;
}

static mtpl_status_t mtpl_sfp_walk_directory(
    mtpl_sfp_builder_t *builder,
    const char *root,
    const char *relative,
    mtpl_storage_mode_t mode,
    unsigned int depth) {
    char *directory;
    mtpl_status_t status = MTPL_STATUS_OK;

    if (depth > MTPL_SFP_MAX_RECURSION_DEPTH) {
        return MTPL_STATUS_LIMIT_EXCEEDED;
    }
    directory = relative[0] == '\0'
        ? mtpl_sfp_strdup(root)
        : mtpl_sfp_join_path(root, relative);
    if (directory == NULL) {
        return MTPL_STATUS_OUT_OF_MEMORY;
    }
#ifdef _WIN32
    {
        WIN32_FIND_DATAW item;
        HANDLE find = INVALID_HANDLE_VALUE;
        char *pattern = mtpl_sfp_join_path(directory, "*");
        wchar_t *wide_pattern;

        if (pattern == NULL) {
            free(directory);
            return MTPL_STATUS_OUT_OF_MEMORY;
        }
        wide_pattern = mtpl_sfp_utf8_to_wide(pattern);
        free(pattern);
        if (wide_pattern == NULL) {
            free(directory);
            return MTPL_STATUS_PATH_ERROR;
        }
        find = FindFirstFileW(wide_pattern, &item);
        free(wide_pattern);
        if (find == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            free(directory);
            return error == ERROR_FILE_NOT_FOUND
                ? MTPL_STATUS_OK
                : MTPL_STATUS_IO_ERROR;
        }
        do {
            char *name;
            char *child_relative;
            char *child_source;

            if (wcscmp(item.cFileName, L".") == 0 ||
                wcscmp(item.cFileName, L"..") == 0) {
                continue;
            }
            name = mtpl_sfp_wide_to_utf8(item.cFileName);
            if (name == NULL) {
                status = MTPL_STATUS_PATH_ERROR;
                break;
            }
            child_relative = relative[0] == '\0'
                ? mtpl_sfp_strdup(name)
                : mtpl_sfp_join_path(relative, name);
            child_source = mtpl_sfp_join_path(directory, name);
            free(name);
            if (child_relative == NULL || child_source == NULL) {
                free(child_relative);
                free(child_source);
                status = MTPL_STATUS_OUT_OF_MEMORY;
                break;
            }
            if ((item.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                status = MTPL_STATUS_PATH_TRAVERSAL;
            } else if ((item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                status = mtpl_sfp_walk_directory(
                    builder,
                    root,
                    child_relative,
                    mode,
                    depth + 1u);
            } else {
                status = mtpl_sfp_builder_add_file(
                    builder,
                    child_relative,
                    child_source,
                    mode);
            }
            free(child_relative);
            free(child_source);
            if (status != MTPL_STATUS_OK) {
                break;
            }
        } while (FindNextFileW(find, &item));
        if (status == MTPL_STATUS_OK && GetLastError() != ERROR_NO_MORE_FILES) {
            status = MTPL_STATUS_IO_ERROR;
        }
    if (!FindClose(find) && status == MTPL_STATUS_OK) {
        status = MTPL_STATUS_IO_ERROR;
    }
    }
#else
    {
        DIR *opened = opendir(directory);
        struct dirent *item;

        if (opened == NULL) {
            free(directory);
            return MTPL_STATUS_IO_ERROR;
        }
        for (;;) {
            char *child_relative;
            char *child_source;
            struct stat info;

            errno = 0;
            item = readdir(opened);
            if (item == NULL) {
                if (errno != 0 && status == MTPL_STATUS_OK) {
                    status = MTPL_STATUS_IO_ERROR;
                }
                break;
            }
            if (strcmp(item->d_name, ".") == 0 ||
                strcmp(item->d_name, "..") == 0) {
                continue;
            }
            if (!mtpl_utf8_is_valid(
                    (const uint8_t *)item->d_name,
                    strlen(item->d_name))) {
                status = MTPL_STATUS_PATH_ERROR;
                break;
            }
            child_relative = relative[0] == '\0'
                ? mtpl_sfp_strdup(item->d_name)
                : mtpl_sfp_join_path(relative, item->d_name);
            child_source = mtpl_sfp_join_path(directory, item->d_name);
            if (child_relative == NULL || child_source == NULL) {
                free(child_relative);
                free(child_source);
                status = MTPL_STATUS_OUT_OF_MEMORY;
                break;
            }
            if (lstat(child_source, &info) != 0) {
                status = MTPL_STATUS_IO_ERROR;
            } else if (S_ISLNK(info.st_mode)) {
                status = MTPL_STATUS_PATH_TRAVERSAL;
            } else if (S_ISDIR(info.st_mode)) {
                status = mtpl_sfp_walk_directory(
                    builder,
                    root,
                    child_relative,
                    mode,
                    depth + 1u);
            } else if (S_ISREG(info.st_mode)) {
                status = mtpl_sfp_builder_add_file(
                    builder,
                    child_relative,
                    child_source,
                    mode);
            } else {
                status = MTPL_STATUS_UNSUPPORTED;
            }
            free(child_relative);
            free(child_source);
            if (status != MTPL_STATUS_OK) {
                break;
            }
        }
        if (closedir(opened) != 0 && status == MTPL_STATUS_OK) {
            status = MTPL_STATUS_IO_ERROR;
        }
    }
#endif
    free(directory);
    return status;
}

mtpl_status_t mtpl_sfp_pack_directory(
    const char *input_directory,
    const char *output_path,
    mtpl_storage_mode_t mode,
    const mtpl_crypto_options_t *crypto) {
    mtpl_sfp_builder_t *builder = NULL;
    bool exists;
    bool is_directory;
    bool is_link;
    uint64_t size;
    mtpl_status_t status;

    if (input_directory == NULL || output_path == NULL ||
        input_directory[0] == '\0' || output_path[0] == '\0' ||
        (mode != MTPL_STORAGE_PLAIN && mode != MTPL_STORAGE_ENCRYPTED)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_path_is_valid_utf8(input_directory) ||
        !mtpl_path_is_valid_utf8(output_path)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    status = mtpl_sfp_path_info(
        input_directory,
        &exists,
        &is_directory,
        &is_link,
        &size);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    if (!exists) {
        return MTPL_STATUS_FILE_NOT_FOUND;
    }
    if (!is_directory) {
        return MTPL_STATUS_PATH_ERROR;
    }
    status = mtpl_sfp_builder_create(crypto, &builder);
    if (status == MTPL_STATUS_OK && mode == MTPL_STORAGE_ENCRYPTED &&
        builder->key.data == NULL) {
        status = MTPL_STATUS_KEY_REQUIRED;
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_walk_directory(builder, input_directory, "", mode, 0u);
    }
    if (status == MTPL_STATUS_OK) {
        status = mtpl_sfp_builder_write(builder, output_path);
    }
    mtpl_sfp_builder_destroy(builder);
    return status;
}

mtpl_status_t mtpl_sfp_unpack(
    const char *input_path,
    const char *output_directory,
    const mtpl_crypto_options_t *crypto,
    uint32_t *file_count) {
    mtpl_sfp_reader_t *reader = NULL;
    size_t index;
    mtpl_status_t status;

    if (file_count == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *file_count = 0u;
    if (output_directory == NULL || output_directory[0] == '\0') {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (!mtpl_path_is_valid_utf8(output_directory)) {
        return MTPL_STATUS_PATH_ERROR;
    }
    status = mtpl_sfp_reader_open(input_path, crypto, &reader);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
#ifdef _WIN32
    for (index = 0u; index < reader->entry_count; ++index) {
        if (!mtpl_sfp_path_is_windows_output_compatible(
                reader->entries[index].path)) {
            mtpl_sfp_reader_close(reader);
            return MTPL_STATUS_PATH_ERROR;
        }
    }
#endif
    status = mtpl_sfp_ensure_directory(output_directory, false);
    for (index = 0u;
        status == MTPL_STATUS_OK && index < reader->entry_count;
        ++index) {
        const mtpl_sfp_entry_t *entry = &reader->entries[index];
        char *destination = NULL;
        mtpl_buffer_t data = { 0 };
        FILE *output;

        status = mtpl_sfp_prepare_output_file(
            output_directory,
            entry->path,
            &destination);
        if (status != MTPL_STATUS_OK) {
            break;
        }
        status = mtpl_sfp_reader_read_file(reader, entry->path, &data);
        if (status != MTPL_STATUS_OK) {
            free(destination);
            break;
        }
        output = mtpl_fopen_utf8(destination, "wb");
        if (output == NULL) {
            status = MTPL_STATUS_IO_ERROR;
        } else {
            status = mtpl_sfp_write_exact(output, data.data, data.size);
            if (fclose(output) != 0 && status == MTPL_STATUS_OK) {
                status = MTPL_STATUS_IO_ERROR;
            }
        }
        mtpl_sfp_release_buffer(&data);
        free(destination);
        if (status == MTPL_STATUS_OK) {
            ++*file_count;
        }
    }
    {
        mtpl_status_t close_status = mtpl_sfp_reader_close(reader);
        if (status == MTPL_STATUS_OK) {
            status = close_status;
        }
    }
    return status;
}
