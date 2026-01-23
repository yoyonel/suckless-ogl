#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Safe wrapper around vsnprintf to format strings with bounds checking.
 *
 * This function wraps vsnprintf to ensure that the formatted string fits within
 * the provided buffer. It returns false if the buffer is null, size is 0, or
 * if truncation occurred.
 *
 * @param buf The destination buffer.
 * @param buf_size The size of the destination buffer.
 * @param format The format string.
 * @param ... variable arguments.
 * @return true if formatting succeeded and fit within the buffer, false
 * otherwise.
 */
// L'attribut indique au compilateur que l'argument 3 est le format
// et l'argument 4 le début des variables (pour les warnings)
__attribute__((format(printf, 3, 4))) static inline bool safe_snprintf(
    char* buf, size_t buf_size, const char* format, ...)
{
	if (!buf || !buf_size) {
		return false;
	}

	va_list args;
	va_start(args, format);
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	int result = vsnprintf(buf, buf_size, format, args);
	va_end(args);

	return (result >= 0 && (size_t)result < buf_size);
}

static inline bool check_flag(int value, int flag)
{
	return ((unsigned int)value & (unsigned int)flag) != 0;
}

/**
 * @brief Safe wrapper around calloc.
 *
 * Checks for zero size and returns NULL if num or size are 0.
 * Suppresses common security warnings for verified allocations.
 */
static inline void* safe_calloc(size_t num, size_t size)
{
	if (num == 0 || size == 0) {
		return NULL;
	}
	// NOLINTNEXTLINE
	return calloc(num, size);
}

/**
 * @brief Safe wrapper around memcpy.
 *
 * Checks for null pointers and ensures dest_size >= count.
 * Returns false if bounds check fails.
 */
static inline bool safe_memcpy(void* dest, size_t dest_size, const void* src,
                               size_t count)
{
	if (!dest || !src || dest_size < count) {
		return false;
	}
	// NOLINTNEXTLINE
	memcpy(dest, src, count);
	return true;
}

#endif  // UTILS_H
