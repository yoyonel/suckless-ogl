#include "log.h"

#include "platform/platform_time.h"
#include "platform/platform_utils.h"
#include "tracy_log.h"
#include "utils.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#define LOG_PLATFORM_WINDOWS
#include <windows.h>
// SRWLOCK est l'équivalent moderne, léger et à initialisation statique !
#define LOG_MUTEX_TYPE SRWLOCK
#define LOG_MUTEX_INIT SRWLOCK_INIT
#define LOG_MUTEX_LOCK(m) AcquireSRWLockExclusive(m)
#define LOG_MUTEX_UNLOCK(m) ReleaseSRWLockExclusive(m)
#else
#define LOG_PLATFORM_POSIX
#include <pthread.h>
#define LOG_MUTEX_TYPE pthread_mutex_t
#define LOG_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define LOG_MUTEX_LOCK(m) pthread_mutex_lock(m)
#define LOG_MUTEX_UNLOCK(m) pthread_mutex_unlock(m)
#endif

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

	static LOG_MUTEX_TYPE s_log_mutex = LOG_MUTEX_INIT;

	int64_t sec = 0;
	int64_t nsec = 0;
	platform_get_time_precise(&sec, &nsec);

	struct tm tm_info;
	time_t _timer = (time_t)sec;
#ifdef LOG_PLATFORM_WINDOWS
	localtime_s(&tm_info, &_timer);
#else
	localtime_r(&_timer, &tm_info);
#endif

	char time_buf[TIME_BUFFER_SIZE];
	(void)strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S",
	               &tm_info);

	char prefix[PREFIX_BUFFER_SIZE];
	int32_t pid = platform_get_pid();
	uint64_t tid = platform_get_tid();
	(void)safe_snprintf(prefix, sizeof(prefix),
	                    "%s,%03ld [%d:%lu] - %s - %-8s - ", time_buf,
	                    (long)(nsec / MILLI_DIVISOR), (int)pid,
	                    (unsigned long)tid, tag, level_to_string(level));

	// Formatage du message dans le buffer local (peut être fait hors du
	// mutex)
	va_list args;
	va_start(args, format);
	char msg_buf[MSG_BUFFER_SIZE];
	// NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized,clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	(void)vsnprintf(msg_buf, sizeof(msg_buf), format, args);
	va_end(args);

	// Section critique pour garantir l'ordre d'affichage
	LOG_MUTEX_LOCK(&s_log_mutex);

	FILE* out = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
	(void)fputs(prefix, out);

	if (g_log_callback) {
		g_log_callback(level, tag, msg_buf);
	}

	tracy_log_message(level, msg_buf);

	(void)fputs(msg_buf, out);
	(void)fputs("\n", out);

	LOG_MUTEX_UNLOCK(&s_log_mutex);
}
