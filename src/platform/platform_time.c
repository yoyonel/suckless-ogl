#include "platform/platform_time.h"

#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#define WIN32_EPOCH_OFFSET 116444736000000000ULL
#define WIN32_TICKS_PER_SEC 10000000ULL
#define WIN32_NS_PER_TICK 100ULL
#define US_PER_MS 1000ULL
#define NS_PER_SEC 1000000000ULL

uint64_t platform_get_time_ns(void)
{
#ifdef _WIN32
	LARGE_INTEGER freq;
	LARGE_INTEGER counter;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&counter);
	// Use 1ULL to avoid overflow before division
	return (uint64_t)((counter.QuadPart * NS_PER_SEC) / freq.QuadPart);
#else
	struct timespec time_spec;
	if (clock_gettime(CLOCK_MONOTONIC, &time_spec) != 0) {
		return 0;
	}
	return ((uint64_t)time_spec.tv_sec * NS_PER_SEC) +
	       (uint64_t)time_spec.tv_nsec;
#endif
}

void platform_get_time_precise(int64_t* out_seconds, int64_t* out_nanoseconds)
{
#ifdef _WIN32
	FILETIME file_time;
	GetSystemTimeAsFileTime(&file_time);
	uint64_t time_val = ((uint64_t)file_time.dwHighDateTime << 32) |
	                    file_time.dwLowDateTime;
	// Convert from 100ns intervals since Jan 1, 1601 to Unix epoch
	uint64_t unix_time = (time_val - WIN32_EPOCH_OFFSET);
	*out_seconds = (int64_t)(unix_time / WIN32_TICKS_PER_SEC);
	*out_nanoseconds =
	    (int64_t)((unix_time % WIN32_TICKS_PER_SEC) * WIN32_NS_PER_TICK);
#else
	struct timespec time_spec;
	if (clock_gettime(CLOCK_REALTIME, &time_spec) != 0) {
		*out_seconds = 0;
		*out_nanoseconds = 0;
		return;
	}
	*out_seconds = (int64_t)time_spec.tv_sec;
	*out_nanoseconds = (int64_t)time_spec.tv_nsec;
#endif
}

void platform_sleep_ms(uint32_t milliseconds_to_sleep)
{
#ifdef _WIN32
	Sleep((DWORD)milliseconds_to_sleep);
#else
	usleep(milliseconds_to_sleep * US_PER_MS);
#endif
}
