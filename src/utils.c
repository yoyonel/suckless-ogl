/**
 * @file utils.c
 * @brief Implementation of safe utility wrappers.
 */

#include "utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Use pragma to ignore insecureAPI warnings within this implementation file
 * only, as these functions are the designated safe wrappers for the project. */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

/* NOLINTBEGIN(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
 */

bool safe_snprintf(char* buf, size_t buf_size, const char* format, ...)
{
	if (!buf || !buf_size) {
		return false;
	}

	va_list args;
	va_start(args, format);
	int result = vsnprintf(buf, buf_size, format, args);
	va_end(args);

	return (result >= 0 && (size_t)result < buf_size);
}

bool safe_memcpy(void* dest, size_t dest_size, const void* src, size_t count)
{
	if (!dest || !src || dest_size < count) {
		return false;
	}
	memcpy(dest, src, count);
	return true;
}

bool safe_memset(void* dest, size_t dest_size, int value, size_t count)
{
	if (!dest || dest_size < count) {
		return false;
	}
	memset(dest, value, count);
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

	(void)strncpy(dest, src, copy_len);
	dest[copy_len] = '\0';
}

void safe_strncat(char* dest, size_t dest_size, const char* src)
{
	if (!dest || !dest_size || !src) {
		return;
	}

	size_t current_len = strnlen(dest, dest_size);
	if (current_len >= dest_size - 1) {
		return; /* No space left */
	}

	size_t remaining = dest_size - current_len - 1;
	(void)strncat(dest, src, remaining);
}

/* NOLINTEND(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
 */

#ifdef __clang__
#pragma clang diagnostic pop
#endif
