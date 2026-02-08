/**
 * @file log.h
 * @brief Thread-safe logging utility with timestamps and severity levels.
 */

#ifndef LOG_H
#define LOG_H

/**
 * @enum LogLevel
 * @brief Severity levels for log filtering.
 */
typedef enum {
	LOG_LEVEL_NOTSET = 0, /**< Undefined level. */
	LOG_LEVEL_DEBUG = 10, /**< Fine-grained informational events that are
	                         most useful to debug an application. */
	LOG_LEVEL_INFO =
	    20, /**< Informational messages that highlight the progress of the
	           application at coarse-grained level. */
	LOG_LEVEL_WARNING = 30, /**< Potentially harmful situations. */
	LOG_LEVEL_ERROR = 40,   /**< Error events that might still allow the
	                           application to continue running. */
	LOG_LEVEL_CRITICAL = 50 /**< Very severe error events that will
	                           presumably lead the application to abort. */
} LogLevel;

/**
 * @brief Callback type for log message interception.
 * @param level Sensitivity level.
 * @param tag Category label.
 * @param message The full formatted log message.
 */
typedef void (*LogCallback)(LogLevel level, const char* tag,
                            const char* message);

/**
 * @brief Logs a formatted message.
 *
 * Format: `YYYY-MM-DD HH:MM:SS,mmm - tag - LEVEL - message`
 *
 * @param level Severity level.
 * @param tag Category label (e.g., "RENDER", "INPUT").
 * @param format Printf-style format string.
 * @param ... Arguments for the format string.
 */
__attribute__((format(printf, 3, 4))) void log_message(LogLevel level,
                                                       const char* tag,
                                                       const char* format, ...);

/**
 * @brief Sets a custom callback for log messages.
 * @param callback Function to call for each log message, or NULL to disable.
 */
void log_set_callback(LogCallback callback);

/**
 * @brief Sets the global minimum log level.
 * @param level Levels below this will be ignored.
 */
void log_set_level(LogLevel level);

/**
 * @brief Retrieves the current global log level.
 * @return Active LogLevel.
 */
LogLevel log_get_level(void);

/* Helper macros for easier logging */

/** @brief Log a debug message. */
#define LOG_DEBUG(tag, ...) log_message(LOG_LEVEL_DEBUG, tag, __VA_ARGS__)
/** @brief Log an info message. */
#define LOG_INFO(tag, ...) log_message(LOG_LEVEL_INFO, tag, __VA_ARGS__)
/** @brief Log a warning message. */
#define LOG_WARNING(tag, ...) log_message(LOG_LEVEL_WARNING, tag, __VA_ARGS__)
/** @brief Alias for LOG_WARNING. */
#define LOG_WARN(tag, ...) LOG_WARNING(tag, __VA_ARGS__)
/** @brief Log an error message. */
#define LOG_ERROR(tag, ...) log_message(LOG_LEVEL_ERROR, tag, __VA_ARGS__)
/** @brief Log a critical failure message. */
#define LOG_CRITICAL(tag, ...) log_message(LOG_LEVEL_CRITICAL, tag, __VA_ARGS__)

#endif /* LOG_H */
