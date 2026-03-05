#ifndef SUCKLESS_OGL_PLATFORM_UTILS_H
#define SUCKLESS_OGL_PLATFORM_UTILS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Get the current process ID.
 * @return Process ID.
 */
int32_t platform_get_pid(void);

/**
 * @brief Get the current thread ID.
 * @return Thread ID.
 */
uint64_t platform_get_tid(void);

/**
 * @brief Allocate aligned memory.
 *
 * @param size Size in bytes.
 * @param alignment Alignment in bytes (must be power of 2).
 * @return Pointer to allocated memory, or NULL on failure.
 */
void* platform_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief Free memory allocated with platform_aligned_alloc.
 * @param ptr Pointer to memory.
 */
void platform_aligned_free(void* ptr);

#endif  // SUCKLESS_OGL_PLATFORM_UTILS_H
