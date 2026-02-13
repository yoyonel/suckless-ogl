#ifndef TRACY_HOOKS_H
#define TRACY_HOOKS_H

#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"
#include "tracy_ogl_bridge.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Wrapped malloc with Tracy allocation tracking.
 */
static inline void* tracy_malloc(size_t size)
{
	void* ptr = malloc(size);
	if (ptr) {
		TracyCAlloc(ptr, size);
	}
	return ptr;
}

/**
 * @brief Wrapped free with Tracy allocation tracking.
 */
static inline void tracy_free(void* ptr)
{
	if (ptr) {
		TracyCFree(ptr);
		free(ptr);
	}
}

/**
 * @brief Wrapped calloc with Tracy allocation tracking.
 */
static inline void* tracy_calloc(size_t nmemb, size_t size)
{
	void* ptr = calloc(nmemb, size);
	if (ptr)
		TracyCAlloc(ptr, nmemb * size);
	return ptr;
}

/**
 * @brief Wrapped realloc with Tracy allocation tracking.
 */
static inline void* tracy_realloc(void* ptr, size_t size)
{
	if (ptr) {
		TracyCFree(ptr);
	}
	void* nptr = realloc(ptr, size);
	if (nptr) {
		TracyCAlloc(nptr, size);
	}
	return nptr;
}

/**
 * @brief Wrapped posix_memalign with Tracy allocation tracking.
 */
static inline int tracy_posix_memalign(void** memptr, size_t alignment,
                                       size_t size)
{
	int res = posix_memalign(memptr, alignment, size);
	if (res == 0 && *memptr)
		TracyCAlloc(*memptr, size);
	return res;
}

/**
 * @brief Wrapped strdup with Tracy allocation tracking.
 */
static inline char* tracy_strdup(const char* s)
{
	char* ptr = strdup(s);
	if (ptr) {
		TracyCAlloc(ptr, strlen(ptr) + 1);
	}
	return ptr;
}

/**
 * @brief Wrapped strndup with Tracy allocation tracking.
 */
static inline char* tracy_strndup(const char* s, size_t n)
{
	char* ptr = strndup(s, n);
	if (ptr) {
		TracyCAlloc(ptr, strlen(ptr) + 1);
	}
	return ptr;
}

// Override standard libc functions with Tracy-aware ones
#if !defined(TRACY_HOOKS_ACTIVE) && !defined(__cplusplus)
#define TRACY_HOOKS_ACTIVE
#define malloc(s) tracy_malloc(s)
#define free(p) tracy_free(p)
#define calloc(n, s) tracy_calloc(n, s)
#define realloc(p, s) tracy_realloc(p, s)
#define posix_memalign(m, a, s) tracy_posix_memalign(m, a, s)
#define strdup(s) tracy_strdup(s)
#define strndup(s, n) tracy_strndup(s, n)

// stb_image support
#define STBI_MALLOC(s) tracy_malloc(s)
#define STBI_FREE(p) tracy_free(p)
#define STBI_REALLOC(p, s) tracy_realloc(p, s)
#endif

#endif  // TRACY_ENABLE

#endif  // TRACY_HOOKS_H
