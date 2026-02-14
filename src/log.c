#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // NOLINT
#endif
#include "log.h"

#include "tracy_log.h"
#include "utils.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

enum {
	MILLI_DIVISOR = 1000000,
	PREFIX_BUFFER_SIZE = 128,
	TIME_BUFFER_SIZE = 24,
	MSG_BUFFER_SIZE = 1024
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static LogLevel g_log_level = LOG_LEVEL_INFO;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool g_log_initialized = false;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static LogCallback g_log_callback = NULL;

static LogLevel string_to_level(const char* str)
{
	if (strcasecmp(str, "DEBUG") == 0) {
		return LOG_LEVEL_DEBUG;
	}
	if (strcasecmp(str, "INFO") == 0) {
		return LOG_LEVEL_INFO;
	}
	if (strcasecmp(str, "WARNING") == 0) {
		return LOG_LEVEL_WARNING;
	}
	if (strcasecmp(str, "WARN") == 0) {
		return LOG_LEVEL_WARNING;
	}
	if (strcasecmp(str, "ERROR") == 0) {
		return LOG_LEVEL_ERROR;
	}
	if (strcasecmp(str, "CRITICAL") == 0) {
		return LOG_LEVEL_CRITICAL;
	}
	return LOG_LEVEL_NOTSET;
}

static void log_init(void)
{
	if (g_log_initialized) {
		return;
	}

	const char* env_level = getenv("OGL_LOG_LEVEL");
	if (env_level) {
		LogLevel level = string_to_level(env_level);
		if (level != LOG_LEVEL_NOTSET) {
			g_log_level = level;
		}
	}
	g_log_initialized = true;
}

static const char* level_to_string(LogLevel level)
{
	switch (level) {
		case LOG_LEVEL_DEBUG:
			return "DEBUG";
		case LOG_LEVEL_INFO:
			return "INFO";
		case LOG_LEVEL_WARNING:
			return "WARNING";
		case LOG_LEVEL_ERROR:
			return "ERROR";
		case LOG_LEVEL_CRITICAL:
			return "CRITICAL";
		default:
			return "UNKNOWN";
	}
}

void log_set_level(LogLevel level)
{
	g_log_level = level;
	g_log_initialized = true;  // Manual override bypasses env init
}

void log_set_callback(LogCallback callback)
{
	g_log_callback = callback;
}

LogLevel log_get_level(void)
{
	if (!g_log_initialized) {
		log_init();
	}
	return g_log_level;
}

void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	if (!g_log_initialized) {
		log_init();
	}

	if (level < g_log_level) {
		return;
	}

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
	                    "%s,%03ld [%d:%ld] - %s - %-8s - ", time_buf,
	                    ts_now.tv_nsec / MILLI_DIVISOR, pid, tid, tag,
	                    level_to_string(level));

	FILE* out = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
	(void)fputs(prefix, out);

	va_list args;
	va_start(args, format);

	char msg_buf[MSG_BUFFER_SIZE];
	// NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized,clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	(void)vsnprintf(msg_buf, sizeof(msg_buf), format, args);

	if (g_log_callback) {
		g_log_callback(level, tag, msg_buf);
	}

	tracy_log_message(level, msg_buf);

	// NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
	(void)fputs(msg_buf, out);
	va_end(args);

	(void)fputs("\n", out);
}
