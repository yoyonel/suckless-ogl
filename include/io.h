/**
 * @file io.h
 * @brief Centralized secure file loading utilities.
 */

#ifndef IO_H
#define IO_H

#include <stddef.h>

/**
 * @brief Reads an entire file into a null-terminated string.
 *
 * Performs security checks to prevent path traversal and applies size limits.
 * The returned buffer must be freed by the caller.
 *
 * @param path The path to the file to read.
 * @param max_size Maximum allowed file size (0 for default
 * MAX_SHADER_SOURCE_SIZE).
 * @param out_size Optional pointer to store the actual file size.
 * @return A null-terminated string containing file content, or NULL on error.
 */
char* io_read_file(const char* path, size_t max_size, size_t* out_size);

#endif /* IO_H */
