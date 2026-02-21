#include "log.h"
#include <stdarg.h>
#include <stdio.h>

void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}
void log_set_callback(LogCallback callback)
{
	(void)callback;
}
void log_set_level(LogLevel level)
{
	(void)level;
}
LogLevel log_get_level(void)
{
	return LOG_LEVEL_DEBUG;
}
