#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

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
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	struct tm* tm_info = localtime(&ts.tv_sec);

	char time_buf[24];
	strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

	/* Prepare prefix: TIMESTAMP,mmm - tag - LEVEL -  */
	char prefix[128];
	snprintf(prefix, sizeof(prefix), "%s,%03ld - %s - %-5s - ", time_buf,
	         ts.tv_nsec / 1000000, tag, level_to_string(level));

	/* Print prefix and then the message */
	va_list args;
	va_start(args, format);

	FILE* out = (level == LOG_LEVEL_ERROR) ? stderr : stdout;

	fprintf(out, "%s", prefix);
	vfprintf(out, format, args);
	fprintf(out, "\n");

	va_end(args);
}
