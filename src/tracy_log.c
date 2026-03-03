#include "tracy_log.h"

#include "profiler.h"
#include <string.h>

void tracy_log_message(LogLevel level, const char* msg)
{
	if (!msg) {
		return;
	}

	enum {
		COLOR_WHITE = 0xFFFFFF,
		COLOR_RED = 0xFF0000,
		COLOR_LIGHT_RED = 0xFF5555,
		COLOR_LIGHT_YELLOW = 0xFFFF55,
		COLOR_LIGHT_GREY = 0xAAAAAA
	};

	uint32_t color = COLOR_WHITE;
	switch (level) {
		case LOG_LEVEL_CRITICAL:
			color = COLOR_RED;
			break;
		case LOG_LEVEL_ERROR:
			color = COLOR_LIGHT_RED;
			break;
		case LOG_LEVEL_WARNING:
			color = COLOR_LIGHT_YELLOW;
			break;
		case LOG_LEVEL_DEBUG:
			color = COLOR_LIGHT_GREY;
			break;
		default:
			break;
	}
	PROFILE_MESSAGE_C(msg, strlen(msg), color);
}
