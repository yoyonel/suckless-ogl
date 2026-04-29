#ifndef TRACY_LOG_H
#define TRACY_LOG_H

#include "log.h"

/**
 * @brief Sends a log message to Tracy if enabled.
 *
 * @param level Log level for color mapping.
 * @param msg The message string.
 */
void tracy_log_message(LogLevel level, const char* msg);

#endif /* TRACY_LOG_H */
