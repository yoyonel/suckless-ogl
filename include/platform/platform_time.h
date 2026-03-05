#ifndef SUCKLESS_OGL_PLATFORM_TIME_H
#define SUCKLESS_OGL_PLATFORM_TIME_H

#include <stdint.h>

/**
 * @brief Get current time in nanoseconds since an arbitrary starting point.
 * @return Nanoseconds.
 */
uint64_t platform_get_time_ns(void);

/**
 * @brief Get current time in seconds and nanoseconds.
 *
 * @param out_seconds Output seconds.
 * @param out_nanoseconds Output nanoseconds.
 */
void platform_get_time_precise(int64_t* out_seconds, int64_t* out_nanoseconds);

/**
 * @brief Sleep for a specified amount of milliseconds.
 * @param milliseconds_to_sleep Milliseconds to sleep.
 */
void platform_sleep_ms(uint32_t milliseconds_to_sleep);

#endif  // SUCKLESS_OGL_PLATFORM_TIME_H
