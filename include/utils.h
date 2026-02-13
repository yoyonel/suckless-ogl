/**
 * @file utils.h
 * @brief Zero-overhead utility functions and RAII cleanup helpers.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TRACY_ENABLE
#include "tracy_memory.h"
#endif

/**
 * @brief Safe wrapper around vsnprintf to format strings with bounds checking.
 * @param buf Destination buffer.
 * @param buf_size Buffer capacity.
 * @param format Printf-style format string.
 * @return true if string was fully written, false if truncated or error.
 */
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

/**
 * @brief Bitwise flag check helper.
 */
static inline bool check_flag(int value, int flag)
{
	return ((unsigned int)value & (unsigned int)flag) != 0;
}

static inline void* safe_calloc(size_t num, size_t size)
{
	if (num == 0 || size == 0) {
		return NULL;
	}
	return calloc(num, size);
}

/**
 * @brief memcpy wrapper with bounds checking.
 */
static inline bool safe_memcpy(void* dest, size_t dest_size, const void* src,
                               size_t count)
{
	if (!dest || !src || dest_size < count) {
		return false;
	}
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	memcpy(dest, src, count);
	return true;
}

/**
 * @brief Safe wrapper around strncpy to ensure null-termination.
 * @param dest Destination buffer.
 * @param dest_size Size of destination buffer.
 * @param src Source string.
 * @param src_size Max characters to copy (or just use sizeof(dest)).
 */
static inline void safe_strncpy(char* dest, size_t dest_size, const char* src,
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

/**
 * @brief Safe wrapper around strncat to ensure bounds safety.
 * @param dest Destination buffer.
 * @param dest_size Total size of destination buffer.
 * @param src Source string.
 */
static inline void safe_strncat(char* dest, size_t dest_size, const char* src)
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

/**
 * @brief RAII callback for `FILE*`.
 */
static inline void cleanup_file(FILE** file_ptr)
{
	if (file_ptr && *file_ptr) {
		(void)fclose(*file_ptr);
	}
}

/** @brief Macro to define a `FILE*` that closes itself at scope exit. */
#define CLEANUP_FILE __attribute__((cleanup(cleanup_file)))

/**
 * @brief Satisfies Static Analyzers for file resource management.
 */
#ifdef __clang_analyzer__
static inline void raii_satisfy_analyzer_file(
    FILE* f)  // NOLINT(readability-identifier-length)
{
	if (f) {
		(void)fclose(f);
	}
}
#define RAII_SATISFY_FILE(f) raii_satisfy_analyzer_file(f)
#else
#define RAII_SATISFY_FILE(f) (void)0
#endif

static inline void cleanup_free(void* ptr_ptr)
{
	void** ptr = (void**)ptr_ptr;
	if (ptr && *ptr) {
		free(*ptr);
	}
}

/** @brief Macro to define a pointer that frees itself at scope exit. */
#define CLEANUP_FREE __attribute__((cleanup(cleanup_free)))

/**
 * @brief Satisfies Static Analyzers for memory resource management.
 */
#ifdef __clang_analyzer__
static inline void raii_satisfy_analyzer_free(
    void* p)  // NOLINT(readability-identifier-length)
{
	free(p);
}
#define RAII_SATISFY_FREE(p) raii_satisfy_analyzer_free(p)
#else
#define RAII_SATISFY_FREE(p) (void)0
#endif

/**
 * @brief Transfers ownership of an RAII-managed variable to the caller.
 *
 * Sets the local variable to NULL to prevent the `cleanup` attribute from
 * triggering.
 */
#define TRANSFER_OWNERSHIP(ptr)                   \
	({                                        \
		__typeof__(ptr) _tmp_ptr = (ptr); \
		(ptr) = 0;                        \
		_tmp_ptr;                         \
	})

#endif /* UTILS_H */
