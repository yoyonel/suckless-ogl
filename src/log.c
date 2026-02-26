#include "log.h"

#include "tracy_log.h"
#include "utils.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"
#endif
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct LogState {
	LogLevel level;
	bool initialized;
	LogCallback callback;
};

enum {
	MILLI_DIVISOR = 1000000,
	PREFIX_BUFFER_SIZE = 128,
	TIME_BUFFER_SIZE = 24,
	MSG_BUFFER_SIZE = 1024
};

static struct LogState* get_log_state(void)
{
	static struct LogState state = {LOG_LEVEL_INFO, false, NULL};
	return &state;
}

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
	struct LogState* state = get_log_state();
	if (state->initialized) {
		return;
	}

	const char* env_level = getenv("OGL_LOG_LEVEL");
	if (env_level) {
		LogLevel level = string_to_level(env_level);
		if (level != LOG_LEVEL_NOTSET) {
			state->level = level;
		}
	}
	state->initialized = true;
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
	struct LogState* state = get_log_state();
	state->level = level;
	state->initialized = true;  // Manual override bypasses env init
}

void log_set_callback(LogCallback callback)
{
	get_log_state()->callback = callback;
}

LogLevel log_get_level(void)
{
	struct LogState* state = get_log_state();
	if (!state->initialized) {
		log_init();
	}
	return state->level;
}

void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	if (!get_log_state()->initialized) {
		log_init();
	}

	if (level < get_log_state()->level) {
		return;
	}

	struct timespec ts_now = {0, 0};
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

	va_list args = {0};
	va_start(args, format);

	char msg_buf[MSG_BUFFER_SIZE];
	(void)safe_vsnprintf(msg_buf, sizeof(msg_buf), format, args);

	struct LogState* state = get_log_state();
	if (state->callback) {
		state->callback(level, tag, msg_buf);
	}

	tracy_log_message(level, msg_buf);

	(void)fputs(msg_buf, out);
	va_end(args);

	(void)fputs("\n", out);
}
