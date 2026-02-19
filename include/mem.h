#ifndef MEM_H
#define MEM_H

#include <stdlib.h>
#include <string.h>

#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

static inline void* tracy_malloc(size_t size)
{
	void* ptr = NULL;
	ptr = malloc(size);
	if (ptr) {
		TracyCAlloc(ptr, size);
	}
	return ptr;
}

static inline void* tracy_calloc(size_t num, size_t size)
{
	void* ptr = NULL;
	ptr = calloc(num, size);
	if (ptr) {
		TracyCAlloc(ptr, num * size);
	}
	return ptr;
}

static inline void* tracy_realloc(void* ptr, size_t size)
{
	if (ptr) {
		TracyCFree(ptr);
	}
	void* new_ptr = NULL;
	new_ptr = realloc(ptr, size);
	if (new_ptr) {
		TracyCAlloc(new_ptr, size);
	}
	return new_ptr;
}

static inline void tracy_free(void* ptr)
{
	if (ptr) {
		TracyCFree(ptr);
		free(ptr);
	}
}

static inline char* tracy_strdup(const char* str)
{
	char* ptr = NULL;
	ptr = strdup(str);
	if (ptr) {
		TracyCAlloc(ptr, strlen(ptr) + 1);
	}
	return ptr;
}

static inline int tracy_posix_memalign(void** memptr, size_t alignment,
                                       size_t size)
{
	int res = posix_memalign(memptr, alignment, size);
	if (res == 0 && *memptr) {
		TracyCAlloc(*memptr, size);
	}
	return res;
}

static inline void* tracy_aligned_alloc(size_t alignment, size_t size)
{
	void* ptr = NULL;
	ptr = aligned_alloc(alignment, size);
	if (ptr) {
		TracyCAlloc(ptr, size);
	}
	return ptr;
}

#define malloc(x) tracy_malloc(x)
#define calloc(x, y) tracy_calloc(x, y)
#define realloc(x, y) tracy_realloc(x, y)
#define free(x) tracy_free(x)
#define strdup(x) tracy_strdup(x)
#define posix_memalign(x, y, z) tracy_posix_memalign(x, y, z)
#define aligned_alloc(x, y) tracy_aligned_alloc(x, y)

#pragma GCC diagnostic pop

#endif  // TRACY_ENABLE

#endif  // MEM_H
