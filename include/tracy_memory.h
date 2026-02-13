#ifndef TRACY_MEMORY_H
#define TRACY_MEMORY_H

#include <stdlib.h>
#include <string.h>

#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"

/* We use the (function_name) syntax to ensure we call the real function
   from the C library even if a macro with the same name exists. */

static inline void* tracy_malloc(size_t size)
{
	void* ptr = (malloc)(size);
	if (ptr) {
		TracyCAlloc(ptr, size);
	}
	return ptr;
}

static inline void tracy_free(void* ptr)
{
	if (ptr) {
		TracyCFree(ptr);
		(free)(ptr);
	}
}

static inline void* tracy_realloc(void* ptr, size_t size)
{
	if (ptr) {
		TracyCFree(ptr);
	}
	void* new_ptr = (realloc)(ptr, size);
	if (new_ptr) {
		TracyCAlloc(new_ptr, size);
	}
	return new_ptr;
}

static inline void* tracy_calloc(size_t num, size_t size)
{
	void* ptr = (calloc)(num, size);
	if (ptr) {
		TracyCAlloc(ptr, num * size);
	}
	return ptr;
}

static inline char* tracy_strdup(const char* str)
{
	char* ptr = (strdup)(str);
	if (ptr) {
		TracyCAlloc(ptr, strlen(ptr) + 1);
	}
	return ptr;
}

static inline void* tracy_aligned_alloc(size_t alignment, size_t size)
{
	void* ptr = (aligned_alloc)(alignment, size);
	if (ptr) {
		TracyCAlloc(ptr, size);
	}
	return ptr;
}

static inline int tracy_posix_memalign(void** memptr, size_t alignment,
                                       size_t size)
{
	int res = (posix_memalign)(memptr, alignment, size);
	if (res == 0 && *memptr) {
		TracyCAlloc(*memptr, size);
	}
	return res;
}

/* Macro overrides to capture ALL allocations in the project */
#define malloc(s) tracy_malloc(s)
#define free(p) tracy_free(p)
#define realloc(p, s) tracy_realloc(p, s)
#define calloc(n, s) tracy_calloc(n, s)
#define strdup(s) tracy_strdup(s)
#define posix_memalign(p, a, s) tracy_posix_memalign(p, a, s)
#define aligned_alloc(a, s) tracy_aligned_alloc(a, s)

#else

#define tracy_malloc malloc
#define tracy_free free
#define tracy_realloc realloc
#define tracy_calloc calloc
#define tracy_strdup strdup

#endif

#endif /* TRACY_MEMORY_H */
