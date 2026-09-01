#ifndef MTPL_INTERNAL_FILE_IO_H
#define MTPL_INTERNAL_FILE_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <mtpl/status.h>

bool mtpl_utf8_is_valid(const uint8_t *text, size_t size);
bool mtpl_path_is_valid_utf8(const char *path);
#ifdef _WIN32
wchar_t *mtpl_path_to_wide(const char *path);
#endif
FILE *mtpl_fopen_utf8(const char *path, const char *mode);
mtpl_status_t mtpl_path_exists_utf8(const char *path, bool *exists);
mtpl_status_t mtpl_remove_utf8(const char *path);
mtpl_status_t mtpl_commit_file_utf8(
    const char *temporary_path,
    const char *destination_path);
mtpl_status_t mtpl_file_is_same(FILE *first, FILE *second, bool *same);
mtpl_status_t mtpl_file_get_size(FILE *file, uint64_t *size);
mtpl_status_t mtpl_file_seek(FILE *file, uint64_t offset);
mtpl_status_t mtpl_file_tell(FILE *file, uint64_t *offset);

#endif
