#include "platform/platform_utils.h"

#include <stdlib.h>

#ifdef __linux__
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

int32_t platform_get_pid(void)
{
#ifdef _WIN32
	return (int32_t)GetCurrentProcessId();
#else
	return (int32_t)getpid();
#endif
}

uint64_t platform_get_tid(void)
{
#ifdef __linux__
	return (uint64_t)syscall(SYS_gettid);
#elif defined(_WIN32)
	return (uint64_t)GetCurrentThreadId();
#elif defined(__APPLE__)
	uint64_t tid;
	pthread_threadid_np(NULL, &tid);
	return tid;
#else
	return 0;  // Fallback
#endif
}

void* platform_aligned_alloc(size_t size, size_t alignment)
{
	void* ptr = NULL;
#ifdef _WIN32
	ptr = _aligned_malloc(size, alignment);
#else
	if (posix_memalign(&ptr, alignment, size) != 0) {
		return NULL;
	}
#endif
	return ptr;
}

void platform_aligned_free(void* ptr)
{
#ifdef _WIN32
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}
