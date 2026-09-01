#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "file_io.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

bool mtpl_utf8_is_valid(const uint8_t *text, size_t size) {
    size_t index = 0u;

    if (text == NULL && size != 0u) {
        return false;
    }
    while (index < size) {
        uint8_t first = text[index++];
        uint32_t value;
        size_t continuation;

        if (first <= 0x7fu) {
            if (first == 0u) {
                return false;
            }
            continue;
        }
        if (first >= 0xc2u && first <= 0xdfu) {
            value = first & 0x1fu;
            continuation = 1u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            value = first & 0x0fu;
            continuation = 2u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            value = first & 0x07u;
            continuation = 3u;
        } else {
            return false;
        }
        if (continuation > size - index) {
            return false;
        }
        while (continuation-- != 0u) {
            uint8_t next = text[index++];
            if ((next & 0xc0u) != 0x80u) {
                return false;
            }
            value = (value << 6u) | (next & 0x3fu);
        }
        if ((value >= 0xd800u && value <= 0xdfffu) ||
            value > 0x10ffffu || value < 0x80u ||
            (value < 0x800u && first >= 0xe0u) ||
            (value < 0x10000u && first >= 0xf0u)) {
            return false;
        }
    }
    return true;
}

bool mtpl_path_is_valid_utf8(const char *path) {
    return path != NULL && path[0] != '\0' &&
        mtpl_utf8_is_valid((const uint8_t *)path, strlen(path));
}

#ifdef _WIN32
static wchar_t *mtpl_utf8_to_wide(const char *value) {
    int length;
    wchar_t *wide;

    if (value == NULL) {
        return NULL;
    }
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0);
    if (length <= 0) {
        return NULL;
    }
    wide = (wchar_t *)malloc((size_t)length * sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, wide, length) == 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static bool mtpl_wide_is_separator(wchar_t value) {
    return value == L'\\' || value == L'/';
}

wchar_t *mtpl_path_to_wide(const char *path) {
    static const wchar_t extended_prefix[] = L"\\\\?\\";
    static const wchar_t unc_prefix[] = L"\\\\?\\UNC\\";
    wchar_t *wide = mtpl_utf8_to_wide(path);
    wchar_t *absolute;
    wchar_t *extended;
    DWORD required;
    DWORD written;
    size_t length;
    size_t index;
    size_t prefix_size;
    size_t source_offset;

    if (wide == NULL || wide[0] == L'\0') {
        free(wide);
        return NULL;
    }
    for (index = 0u; wide[index] != L'\0'; ++index) {
        if (wide[index] == L'/') {
            wide[index] = L'\\';
        }
    }
    if (wcsncmp(wide, extended_prefix, 4u) == 0) {
        return wide;
    }
    if (wcsncmp(wide, L"\\\\.\\", 4u) == 0) {
        free(wide);
        return NULL;
    }

    required = GetFullPathNameW(wide, 0u, NULL, NULL);
    if (required == 0u || required > SIZE_MAX / sizeof(*absolute)) {
        free(wide);
        return NULL;
    }
    absolute = (wchar_t *)malloc((size_t)required * sizeof(*absolute));
    if (absolute == NULL) {
        free(wide);
        return NULL;
    }
    written = GetFullPathNameW(wide, required, absolute, NULL);
    free(wide);
    if (written == 0u || written >= required) {
        free(absolute);
        return NULL;
    }
    for (index = 0u; absolute[index] != L'\0'; ++index) {
        if (absolute[index] == L'/') {
            absolute[index] = L'\\';
        }
    }

    length = wcslen(absolute);
    if (length >= 2u && mtpl_wide_is_separator(absolute[0]) &&
        mtpl_wide_is_separator(absolute[1])) {
        prefix_size = 8u;
        source_offset = 2u;
    } else if (length >= 3u && absolute[1] == L':' &&
        mtpl_wide_is_separator(absolute[2])) {
        prefix_size = 4u;
        source_offset = 0u;
    } else {
        free(absolute);
        return NULL;
    }
    if (length - source_offset >
        SIZE_MAX / sizeof(*extended) - prefix_size - 1u) {
        free(absolute);
        return NULL;
    }
    extended = (wchar_t *)malloc(
        (prefix_size + length - source_offset + 1u) * sizeof(*extended));
    if (extended == NULL) {
        free(absolute);
        return NULL;
    }
    if (source_offset == 2u) {
        memcpy(extended, unc_prefix, prefix_size * sizeof(*extended));
    } else {
        memcpy(extended, extended_prefix, prefix_size * sizeof(*extended));
    }
    memcpy(
        extended + prefix_size,
        absolute + source_offset,
        (length - source_offset + 1u) * sizeof(*extended));
    free(absolute);
    return extended;
}
#endif

FILE *mtpl_fopen_utf8(const char *path, const char *mode) {
#ifdef _WIN32
    FILE *file;
    wchar_t *wide_path = mtpl_path_to_wide(path);
    wchar_t *wide_mode = mtpl_utf8_to_wide(mode);
    if (wide_path == NULL || wide_mode == NULL) {
        free(wide_path);
        free(wide_mode);
        return NULL;
    }
    file = _wfopen(wide_path, wide_mode);
    free(wide_path);
    free(wide_mode);
    return file;
#else
    return fopen(path, mode);
#endif
}

mtpl_status_t mtpl_path_exists_utf8(const char *path, bool *exists) {
    if (exists != NULL) {
        *exists = false;
    }
    if (!mtpl_path_is_valid_utf8(path) || exists == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    {
        wchar_t *wide_path = mtpl_path_to_wide(path);
        DWORD attributes;
        DWORD error;
        if (wide_path == NULL) {
            return MTPL_STATUS_PATH_ERROR;
        }
        attributes = GetFileAttributesW(wide_path);
        free(wide_path);
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            *exists = true;
            return MTPL_STATUS_OK;
        }
        error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return MTPL_STATUS_OK;
        }
        return MTPL_STATUS_IO_ERROR;
    }
#else
    {
        struct stat information;
        if (stat(path, &information) == 0) {
            *exists = true;
            return MTPL_STATUS_OK;
        }
        return errno == ENOENT || errno == ENOTDIR
            ? MTPL_STATUS_OK
            : MTPL_STATUS_IO_ERROR;
    }
#endif
}

mtpl_status_t mtpl_remove_utf8(const char *path) {
    if (!mtpl_path_is_valid_utf8(path)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    {
        wchar_t *wide_path = mtpl_path_to_wide(path);
        int result;
        if (wide_path == NULL) {
            return MTPL_STATUS_PATH_ERROR;
        }
        result = _wremove(wide_path);
        free(wide_path);
        if (result == 0 || errno == ENOENT) {
            return MTPL_STATUS_OK;
        }
        return MTPL_STATUS_IO_ERROR;
    }
#else
    if (unlink(path) == 0 || errno == ENOENT) {
        return MTPL_STATUS_OK;
    }
    return MTPL_STATUS_IO_ERROR;
#endif
}

mtpl_status_t mtpl_commit_file_utf8(
    const char *temporary_path,
    const char *destination_path) {
    if (!mtpl_path_is_valid_utf8(temporary_path) ||
        !mtpl_path_is_valid_utf8(destination_path)) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    {
        wchar_t *wide_temporary = mtpl_path_to_wide(temporary_path);
        wchar_t *wide_destination = mtpl_path_to_wide(destination_path);
        DWORD error;
        if (wide_temporary == NULL || wide_destination == NULL) {
            free(wide_temporary);
            free(wide_destination);
            return MTPL_STATUS_PATH_ERROR;
        }
        if (MoveFileExW(
                wide_temporary,
                wide_destination,
                MOVEFILE_WRITE_THROUGH) != 0) {
            free(wide_temporary);
            free(wide_destination);
            return MTPL_STATUS_OK;
        }
        error = GetLastError();
        free(wide_temporary);
        free(wide_destination);
        return error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS
            ? MTPL_STATUS_ALREADY_EXISTS
            : MTPL_STATUS_IO_ERROR;
    }
#else
    if (link(temporary_path, destination_path) != 0) {
        return errno == EEXIST
            ? MTPL_STATUS_ALREADY_EXISTS
            : MTPL_STATUS_IO_ERROR;
    }
    (void)unlink(temporary_path);
    return MTPL_STATUS_OK;
#endif
}

mtpl_status_t mtpl_file_is_same(FILE *first, FILE *second, bool *same) {
    if (same != NULL) {
        *same = false;
    }
    if (first == NULL || second == NULL || same == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    if (first == second) {
        *same = true;
        return MTPL_STATUS_OK;
    }
#ifdef _WIN32
    {
        int first_descriptor = _fileno(first);
        int second_descriptor = _fileno(second);
        intptr_t first_value;
        intptr_t second_value;
        BY_HANDLE_FILE_INFORMATION first_info;
        BY_HANDLE_FILE_INFORMATION second_info;

        if (first_descriptor < 0 || second_descriptor < 0) {
            return MTPL_STATUS_IO_ERROR;
        }
        first_value = _get_osfhandle(first_descriptor);
        second_value = _get_osfhandle(second_descriptor);
        if (first_value == -1 || second_value == -1) {
            return MTPL_STATUS_IO_ERROR;
        }
        if (!GetFileInformationByHandle((HANDLE)first_value, &first_info) ||
            !GetFileInformationByHandle((HANDLE)second_value, &second_info)) {
            return MTPL_STATUS_IO_ERROR;
        }
        *same = first_info.dwVolumeSerialNumber ==
                second_info.dwVolumeSerialNumber &&
            first_info.nFileIndexHigh == second_info.nFileIndexHigh &&
            first_info.nFileIndexLow == second_info.nFileIndexLow;
    }
#else
    {
        int first_descriptor = fileno(first);
        int second_descriptor = fileno(second);
        struct stat first_info;
        struct stat second_info;

        if (first_descriptor < 0 || second_descriptor < 0 ||
            fstat(first_descriptor, &first_info) != 0 ||
            fstat(second_descriptor, &second_info) != 0) {
            return MTPL_STATUS_IO_ERROR;
        }
        *same = first_info.st_dev == second_info.st_dev &&
            first_info.st_ino == second_info.st_ino;
    }
#endif
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_file_get_size(FILE *file, uint64_t *size) {
    uint64_t current;
    uint64_t end_size;
    mtpl_status_t status;

    if (size == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *size = 0u;
    if (file == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    status = mtpl_file_tell(file, &current);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
#ifdef _WIN32
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        (void)mtpl_file_seek(file, current);
        return MTPL_STATUS_IO_ERROR;
    }
    {
        __int64 end = _ftelli64(file);
        if (end < 0) {
            (void)mtpl_file_seek(file, current);
            return MTPL_STATUS_IO_ERROR;
        }
        end_size = (uint64_t)end;
    }
#else
    if (fseeko(file, 0, SEEK_END) != 0) {
        (void)mtpl_file_seek(file, current);
        return MTPL_STATUS_IO_ERROR;
    }
    {
        off_t end = ftello(file);
        if (end < 0) {
            (void)mtpl_file_seek(file, current);
            return MTPL_STATUS_IO_ERROR;
        }
        end_size = (uint64_t)end;
    }
#endif
    status = mtpl_file_seek(file, current);
    if (status != MTPL_STATUS_OK) {
        return status;
    }
    *size = end_size;
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_file_seek(FILE *file, uint64_t offset) {
    if (file == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    if (offset > INT64_MAX || _fseeki64(file, (__int64)offset, SEEK_SET) != 0) {
        return MTPL_STATUS_IO_ERROR;
    }
#else
    if (offset > INT64_MAX || fseeko(file, (off_t)offset, SEEK_SET) != 0) {
        return MTPL_STATUS_IO_ERROR;
    }
#endif
    return MTPL_STATUS_OK;
}

mtpl_status_t mtpl_file_tell(FILE *file, uint64_t *offset) {
    if (offset == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
    *offset = 0u;
    if (file == NULL) {
        return MTPL_STATUS_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    {
        __int64 position = _ftelli64(file);
        if (position < 0) {
            return MTPL_STATUS_IO_ERROR;
        }
        *offset = (uint64_t)position;
    }
#else
    {
        off_t position = ftello(file);
        if (position < 0) {
            return MTPL_STATUS_IO_ERROR;
        }
        *offset = (uint64_t)position;
    }
#endif
    return MTPL_STATUS_OK;
}
