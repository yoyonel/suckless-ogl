#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif
#include "log.h"

#include "utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>  // IWYU pragma: keep
#include <unistd.h>

enum {
	MILLI_DIVISOR = 1000000,
	PREFIX_BUFFER_SIZE = 128,
	TIME_BUFFER_SIZE = 24
};

static const char* level_to_string(LogLevel level)
{
	switch (level) {
		case LOG_LEVEL_DEBUG:
			return "DEBUG";
		case LOG_LEVEL_INFO:
			return "INFO";
		case LOG_LEVEL_WARN:
			return "WARN";
		case LOG_LEVEL_ERROR:
			return "ERROR";
		default:
			return "UNKNOWN";
	}
}

void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	struct timespec ts_now = {0, 0};
	// NOLINTNEXTLINE(misc-include-cleaner)
	if (clock_gettime(CLOCK_REALTIME, &ts_now) != 0) {
		ts_now.tv_sec = 0;
		ts_now.tv_nsec = 0;
	}

	struct tm* tm_info = localtime(&ts_now.tv_sec);
	char time_buf[TIME_BUFFER_SIZE];
	(void)strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S",
	               tm_info);

	char prefix[PREFIX_BUFFER_SIZE];
	pid_t pid = getpid();
	long tid = syscall(SYS_gettid);
	(void)safe_snprintf(prefix, sizeof(prefix),
	                    "%s,%03ld [%d:%ld] - %s - %-5s - ", time_buf,
	                    ts_now.tv_nsec / MILLI_DIVISOR, pid, tid, tag,
	                    level_to_string(level));

	FILE* out = (level == LOG_LEVEL_ERROR) ? stderr : stdout;
	(void)fputs(prefix, out);

	va_list args;
	va_start(args, format);
	// NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
	(void)vfprintf(out, format, args);
	va_end(args);

	(void)fputs("\n", out);
}
