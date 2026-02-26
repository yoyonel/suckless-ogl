/**
 * @file utils.c
 * @brief Implementation of utility functions.
 */

#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
/* Suppress "function 'vsnprintf' is insecure" and similar analyzer warnings
 * because this file implements the safe wrappers themselves. */
#pragma clang diagnostic ignored "-Wformat-security"
#endif
#if defined(__clang__) && !defined(__APPLE__)
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#endif

bool safe_snprintf(char* buf, size_t buf_size, const char* format, ...)
{
	if (!buf || !buf_size || !format) {
		return false;
	}

	va_list args = {0};
	va_start(args, format);
	bool result = safe_vsnprintf(buf, buf_size, format, args);
	va_end(args);

	return result;
}

bool safe_vsnprintf(char* buf, size_t buf_size, const char* format,
                    va_list args)
{
	if (!buf || !buf_size || !format) {
		return false;
	}

	va_list args_copy = {0};
	va_copy(args_copy, args);
	int result = vsnprintf(buf, buf_size, format, args_copy);
	va_end(args_copy);
	return (result >= 0 && (size_t)result < buf_size);
}

void* safe_calloc(size_t num, size_t size)
{
	if (num == 0 || size == 0) {
		return NULL;
	}
	return calloc(num, size);
}

bool safe_memcpy(void* dest, size_t dest_size, const void* src, size_t count)
{
	if (!dest || !src || dest_size < count) {
		return false;
	}
	__builtin_memcpy(dest, src, count);
	return true;
}

bool safe_memset(void* dest, size_t dest_size, int value, size_t count)
{
	if (!dest || dest_size < count) {
		return false;
	}
	__builtin_memset(dest, value, count);
	return true;
}

void safe_strncpy(char* dest, size_t dest_size, const char* src,
                  size_t src_size)
{
	if (!dest || !dest_size || !src) {
		return;
	}

	size_t copy_len = src_size;
	if (copy_len >= dest_size) {
		copy_len = dest_size - 1;
	}

	__builtin_strncpy(dest, src, copy_len);
	dest[copy_len] = '\0';
}

void safe_strncat(char* dest, size_t dest_size, const char* src)
{
	if (!dest || !dest_size || !src) {
		return;
	}

	size_t current_len = strnlen(dest, dest_size);
	if (current_len >= dest_size - 1) {
		return;  // No space left
	}

	size_t remaining = dest_size - current_len - 1;
	size_t src_len = strlen(src);
	if (src_len > remaining) {
		src_len = remaining;
	}
	__builtin_memcpy(dest + current_len, src, src_len);
	dest[current_len + src_len] = '\0';
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

bool is_safe_filename(const char* filename)
{
	if (!filename) {
		return false;
	}
	/* Check for directory traversal (..) */
	if (strstr(filename, "..") != NULL) {
		return false;
	}
	if (strcmp(filename, ".") == 0) {
		return false;
	}
	/* Check for path separators (Linux/Windows) */
	if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
		return false;
	}
	return true;
}

bool is_safe_relative_path(const char* path)
{
	if (!path) {
		return false;
	}
	/* No parent directory traversal */
	if (strstr(path, "..") != NULL) {
		return false;
	}
	/* No absolute paths */
	if (path[0] == '/') {
		return false;
	}
	/* No Windows-style backslashes */
	if (strchr(path, '\\') != NULL) {
		return false;
	}
	/* No Windows-style drive letters or colon-based protocols */
	if (strstr(path, ":") != NULL) {
		return false;
	}
	return true;
}

#ifdef __clang_analyzer__
void raii_satisfy_analyzer_file(FILE* file_ptr)
{
	if (file_ptr) {
		(void)fclose(file_ptr);
	}
}

void raii_satisfy_analyzer_free(void* ptr)
{
	free(ptr);
}
#endif
