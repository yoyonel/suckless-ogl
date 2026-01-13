#ifndef LOG_H
#define LOG_H

typedef enum {
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR
} LogLevel;

/**
 * Log a message with a specific level and tag.
 * Format: YYYY-MM-DD HH:MM:SS,mmm - tag - LEVEL - message
 */
void log_message(LogLevel level, const char* tag, const char* format, ...);

/* Helper macros for easier logging */
#define LOG_DEBUG(tag, ...) log_message(LOG_LEVEL_DEBUG, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) log_message(LOG_LEVEL_INFO, tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) log_message(LOG_LEVEL_WARN, tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) log_message(LOG_LEVEL_ERROR, tag, __VA_ARGS__)

#endif /* LOG_H */
