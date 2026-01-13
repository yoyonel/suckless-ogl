#define _POSIX_C_SOURCE 199309L // NOLINT
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

enum {
	MILLI_DIVISOR = 1000000,
	PREFIX_BUFFER_SIZE = 128,
	TIME_BUFFER_SIZE = 24
};

static const char* level_to_string(LogLevel level);

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
	struct timespec ts_now;
	(void)clock_gettime(CLOCK_REALTIME, &ts_now); // NOLINT

	struct tm* tm_info = localtime(&ts_now.tv_sec);

	char time_buf[TIME_BUFFER_SIZE];
	(void)strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

	/* Prepare prefix: TIMESTAMP,mmm - tag - LEVEL -  */
	char prefix[PREFIX_BUFFER_SIZE];
	/* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
	(void)snprintf(prefix, sizeof(prefix), "%s,%03ld - %s - %-5s - ",
	               time_buf, ts_now.tv_nsec / MILLI_DIVISOR, tag,
	               level_to_string(level));

	/* Print prefix and then the message */
	va_list args;
	va_start(args, format);

	FILE* out = (level == LOG_LEVEL_ERROR) ? stderr : stdout;

	(void)fputs(prefix, out);
	(void)vfprintf(out, format, args);
	(void)fputs("\n", out);

	va_end(args);
}
