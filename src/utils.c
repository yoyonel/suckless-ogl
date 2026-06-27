/**
 * @file utils.c
 * @brief Implementation of utility functions.
 */

#include "utils.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  // for free
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

// NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,
// clang-analyzer-valist.Uninitialized)

int safe_snprintf(char* buf, size_t buf_size, const char* format, ...)
{
	if (!buf || !buf_size) {
		return -1;
	}

	va_list args;
	va_start(args, format);
	int result = vsnprintf(buf, buf_size, format, args);
	va_end(args);

	if (result >= 0 && (size_t)result < buf_size) {
		return result;
	}
	return -1;
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

	unsigned char* d = (unsigned char*)dest;
	const unsigned char* s = (const unsigned char*)src;

	/* Le compilateur auto-vectorisera cette boucle en une copie de blocs
	 * optimisée */
	for (size_t i = 0; i < count; ++i) {
		d[i] = s[i];
	}

	return true;
}

bool safe_memset(void* dest, size_t dest_size, int value, size_t count)
{
	if (!dest || dest_size < count) {
		return false;
	}

	unsigned char* ptr = (unsigned char*)dest;
	/* Le compilateur va auto-vectoriser cette boucle */
	for (size_t i = 0; i < count; ++i) {
		ptr[i] = (unsigned char)value;
	}

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

	size_t len = 0;
	while (len < copy_len && src[len] != '\0') {
		len++;
	}
	memcpy(dest, src, len);
	dest[len] = '\0'; /* Garantie absolue de la terminaison nulle */
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
	(void)strncat(dest, src, remaining);
}

// NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,
// clang-analyzer-valist.Uninitialized)

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
