/**
 * @file utils.h
 * @brief Zero-overhead utility functions and RAII cleanup helpers.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Helper to securely cast an integer offset to a pointer, often used for
 * VBO/EBO byte offsets.
 * @param offset The byte offset to cast.
 * @return A void pointer representing the offset.
 */
void* utils_buffer_offset(size_t offset);

/**
 * @brief Safe wrapper around vsnprintf to format strings with bounds checking.
 * @param buf Destination buffer.
 * @param buf_size Buffer capacity.
 * @param format Printf-style format string.
 * @return number of characters written (excluding null terminator) on success,
 *         -1 if truncated or error.
 */
__attribute__((format(printf, 3, 4))) int safe_snprintf(char* buf,
                                                        size_t buf_size,
                                                        const char* format,
                                                        ...);

/**
 * @brief Bitwise flag check helper.
 */
static inline bool check_flag(int value, int flag)
{
	return ((unsigned int)value & (unsigned int)flag) != 0;
}

/**
 * @brief calloc wrapper with zero-size check.
 */
void* safe_calloc(size_t num, size_t size);

/**
 * @brief memcpy wrapper with bounds checking.
 */
bool safe_memcpy(void* dest, size_t dest_size, const void* src, size_t count);

/**
 * @brief memset wrapper with bounds checking.
 */
bool safe_memset(void* dest, size_t dest_size, int value, size_t count);

/**
 * @brief Safe wrapper around strncpy to ensure null-termination.
 * @param dest Destination buffer.
 * @param dest_size Size of destination buffer.
 * @param src Source string.
 * @param src_size Max characters to copy (or just use sizeof(dest)).
 */
void safe_strncpy(char* dest, size_t dest_size, const char* src,
                  size_t src_size);

/**
 * @brief Safe wrapper around strncat to ensure bounds safety.
 * @param dest Destination buffer.
 * @param dest_size Total size of destination buffer.
 * @param src Source string.
 */
void safe_strncat(char* dest, size_t dest_size, const char* src);

/**
 * @brief Validates a filename to prevent path traversal and shell injection.
 *
 * Rejects strings containing path separators ('/', '\\') or directory
 * traversal sequences ("..") or current directory (".").
 *
 * @param filename The filename to check.
 * @return true if the filename is safe, false otherwise.
 */
bool is_safe_filename(const char* filename);

/**
 * @brief Validates a relative path to prevent arbitrary file access.
 *
 * Rejects absolute paths, parent directory traversal (".."), and
 * platform-specific path features like backslashes or drive letters.
 *
 * @param path The relative path to check.
 * @return true if the path is safe, false otherwise.
 */
bool is_safe_relative_path(const char* path);

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
void raii_satisfy_analyzer_file(FILE* file_ptr);
#define RAII_SATISFY_FILE(f) raii_satisfy_analyzer_file(f)
#else
#define RAII_SATISFY_FILE(f) (void)0
#endif

/**
 * @brief RAII callback for `free()`.
 */
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
void raii_satisfy_analyzer_free(void* ptr);
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
