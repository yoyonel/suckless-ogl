#include "gl_debug.h"

#include "gl_common.h"
#include "log.h"
#include <stdint.h>
#include <stdio.h>

#define LOG_TAG "OpenGL Debug"

enum {
	DEBUG_HASH_SIZE = 1024,
};

typedef struct {
	GLuint message_id;
	uint32_t count;
} DebugMessageEntry;

static uint32_t hash_id(GLuint message_id)
{
	return message_id % DEBUG_HASH_SIZE;
}

static const char* get_source_str(GLenum source)
{
	switch (source) {
		case GL_DEBUG_SOURCE_API:
			return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
			return "Window System";
		case GL_DEBUG_SOURCE_SHADER_COMPILER:
			return "Shader Compiler";
		case GL_DEBUG_SOURCE_THIRD_PARTY:
			return "Third Party";
		case GL_DEBUG_SOURCE_APPLICATION:
			return "Application";
		case GL_DEBUG_SOURCE_OTHER:
			return "Other";
		default:
			return "Unknown";
	}
}

static const char* get_type_str(GLenum type)
{
	switch (type) {
		case GL_DEBUG_TYPE_ERROR:
			return "Error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			return "Deprecated Behavior";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			return "Undefined Behavior";
		case GL_DEBUG_TYPE_PORTABILITY:
			return "Portability";
		case GL_DEBUG_TYPE_PERFORMANCE:
			return "Performance";
		case GL_DEBUG_TYPE_MARKER:
			return "Marker";
		case GL_DEBUG_TYPE_PUSH_GROUP:
			return "Push Group";
		case GL_DEBUG_TYPE_POP_GROUP:
			return "Pop Group";
		case GL_DEBUG_TYPE_OTHER:
			return "Other";
		default:
			return "Unknown";
	}
}

static const char* get_severity_str(GLenum severity)
{
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			return "High";
		case GL_DEBUG_SEVERITY_MEDIUM:
			return "Medium";
		case GL_DEBUG_SEVERITY_LOW:
			return "Low";
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			return "Notification";
		default:
			return "Unknown";
	}
}

static void APIENTRY gl_debug_callback(GLenum source, GLenum type,
                                       GLuint message_id, GLenum severity,
                                       GLsizei length, const GLchar* message,
                                       const void* user_param)
{
	(void)length;
	(void)user_param;

	static DebugMessageEntry debug_cache[DEBUG_HASH_SIZE] = {0};

	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
		return;
	}

	uint32_t hash_idx = hash_id(message_id);
	DebugMessageEntry* entry = &debug_cache[hash_idx];

	if (entry->message_id != message_id) {
		entry->message_id = message_id;
		entry->count = 0;
	}

	entry->count++;

	/* Log only the first occurrence to avoid flooding, matching Rust
	 * behavior */
	if (entry->count == 1) {
		const char* src_str = get_source_str(source);
		const char* type_str = get_type_str(type);
		const char* sev_str = get_severity_str(severity);

		LOG_WARN(
		    LOG_TAG,
		    "id: 0x%X, source: %s, type: %s, severity: %s, message: %s",
		    message_id, src_str, type_str, sev_str, message);
	}
}

void setup_opengl_debug(void)
{
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	glDebugMessageCallback(gl_debug_callback, NULL);

	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL,
	                      GL_TRUE);

	LOG_INFO(LOG_TAG, "OpenGL Debug Callback initialized");
}
