#include "tracy_log.h"

#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"
#include <string.h>

void tracy_log_message(LogLevel level, const char* msg)
{
	if (!msg) {
		return;
	}

	uint32_t color = 0xFFFFFF;  // Default Info (White)
	switch (level) {
		case LOG_LEVEL_CRITICAL:
			color = 0xFF0000;
			break;
		case LOG_LEVEL_ERROR:
			color = 0xFF5555;
			break;
		case LOG_LEVEL_WARNING:
			color = 0xFFFF55;
			break;
		case LOG_LEVEL_DEBUG:
			color = 0xAAAAAA;
			break;
		default:
			break;
	}
	TracyCMessageC(msg, strlen(msg), color);
}
#else
void tracy_log_message(LogLevel level, const char* msg)
{
	(void)level;
	(void)msg;
}
#endif
