#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

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

#endif  // UTILS_H
