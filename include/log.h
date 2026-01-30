#ifndef LOG_H
#define LOG_H

typedef enum {
	LOG_LEVEL_NOTSET = 0,
	LOG_LEVEL_DEBUG = 10,
	LOG_LEVEL_INFO = 20,
	LOG_LEVEL_WARNING = 30,
	LOG_LEVEL_ERROR = 40,
	LOG_LEVEL_CRITICAL = 50
} LogLevel;

/**
 * Log a message with a specific level and tag.
 * Format: YYYY-MM-DD HH:MM:SS,mmm - tag - LEVEL - message
 */
void log_message(LogLevel level, const char* tag, const char* format, ...);

/**
 * Set the current log level. Messages below this level will be filtered out.
 */
void log_set_level(LogLevel level);

/**
 * Get the current log level.
 */
LogLevel log_get_level(void);

/* Helper macros for easier logging */
#define LOG_DEBUG(tag, ...) log_message(LOG_LEVEL_DEBUG, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) log_message(LOG_LEVEL_INFO, tag, __VA_ARGS__)
#define LOG_WARNING(tag, ...) log_message(LOG_LEVEL_WARNING, tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) LOG_WARNING(tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) log_message(LOG_LEVEL_ERROR, tag, __VA_ARGS__)
#define LOG_CRITICAL(tag, ...) log_message(LOG_LEVEL_CRITICAL, tag, __VA_ARGS__)

#endif /* LOG_H */
