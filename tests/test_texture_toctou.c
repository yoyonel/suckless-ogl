#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock Dependencies
#include "glad/glad.h"
#include "log.h"
#include "mocks/standalone/mock_gl_standalone.h"
#include "mocks/standalone/mock_stb_image_standalone.h"
#include "stb_image.h"
#include "unity.h"

/* Define GL types/constants missing from mock_gl_standalone.h */
typedef unsigned int GLbitfield;
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT 0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020

/* Redirect GL calls to mocks to avoid glad.h conflicts */
/* Redirect GL calls to mocks to avoid glad.h conflicts */
#define glGenBuffers mock_glGenBuffers
#define glBindBuffer mock_glBindBuffer
#define glBufferData mock_glBufferData
#define glMapBufferRange mock_glMapBufferRange
#define glUnmapBuffer mock_glUnmapBuffer
#define glDeleteBuffers mock_glDeleteBuffers
#define glGetError mock_glGetError
#define glGenerateMipmap mock_glGenerateMipmap
#define glTexParameteri mock_glTexParameteri
#define glPixelStorei mock_glPixelStorei
#define glTexStorage2D mock_glTexStorage2D
#define glTexSubImage2D mock_glTexSubImage2D
#define glGenTextures mock_glGenTextures
#define glBindTexture mock_glBindTexture
#define glDeleteTextures mock_glDeleteTextures
#define glGetTexLevelParameteriv mock_glGetTexLevelParameteriv

// Define mocks for log functions
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
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
	return LOG_LEVEL_INFO;
}

// Mock GPU Profiler (used by texture.c TRACE_GPU_SCOPE)
#include "gpu_profiler.h"
void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color)
{
	(void)profiler;
	(void)name;
	(void)color;
}
void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	(void)profiler;
}

// Implement mocks not present in mock_gl_standalone.h or needed by texture.c
void mock_glGenBuffers(GLsizei n, GLuint* buffers)
{
	(void)n;
	if (buffers)
		*buffers = 1;
}
void mock_glBindBuffer(GLenum target, GLuint buffer)
{
	(void)target;
	(void)buffer;
}
void mock_glBufferData(GLenum target, GLsizeiptr size, const void* data,
                       GLenum usage)
{
	(void)target;
	(void)size;
	(void)data;
	(void)usage;
}
void* mock_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                            GLbitfield access)
{
	(void)target;
	(void)offset;
	(void)length;
	(void)access;
	return NULL;
}
GLboolean mock_glUnmapBuffer(GLenum target)
{
	(void)target;
	return GL_TRUE;
}
void mock_glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
	(void)n;
	(void)buffers;
}
// Stub other functions that are macros mapping to mocks, if they aren't in the
// included header (Assuming mock_gl_standalone.h declares them, but we might
// need to define them if it's just a header) Checking file contents of
// mock_gl_standalone.c showed it exists. But we are linking against a
// standalone test. texture.c includes gl_common.h -> glad.h. We are redefining
// glXXX macros. We need to implement mock_glXXX functions.

GLenum mock_glGetError(void)
{
	return GL_NO_ERROR;
}
void mock_glGenerateMipmap(GLenum target)
{
	(void)target;
}
void mock_glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	(void)target;
	(void)pname;
	(void)param;
}
void mock_glPixelStorei(GLenum pname, GLint param)
{
	(void)pname;
	(void)param;
}
void mock_glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                         GLsizei width, GLsizei height)
{
	(void)target;
	(void)levels;
	(void)internalformat;
	(void)width;
	(void)height;
}
void mock_glTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                          GLint yoffset, GLsizei width, GLsizei height,
                          GLenum format, GLenum type, const void* pixels)
{
	(void)target;
	(void)level;
	(void)xoffset;
	(void)yoffset;
	(void)width;
	(void)height;
	(void)format;
	(void)type;
	(void)pixels;
}
void mock_glGenTextures(GLsizei n, GLuint* textures)
{
	(void)n;
	if (textures)
		*textures = 1;
}
void mock_glBindTexture(GLenum target, GLuint texture)
{
	(void)target;
	(void)texture;
}
void mock_glDeleteTextures(GLsizei n, const GLuint* textures)
{
	(void)n;
	(void)textures;
}
void mock_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                                   GLint* params)
{
	(void)target;
	(void)level;
	(void)pname;
	(void)params;
}

// Include source under test
#include "../src/texture.c"

static const char* const TEST_FILENAME = "dummy_toctou.hdr";

void setUp(void)
{
	// Create a dummy file
	FILE* f = fopen(TEST_FILENAME, "w");
	if (f) {
		fprintf(f, "DUMMY");
		fclose(f);
	}
	// mock_gl_reset_calls(); // Not available/needed in this manual mock
	// setup
	mock_stbi_set_toctou_simulation(0);
	mock_stbi_set_info_dimensions(10, 10, 4);
}

void tearDown(void)
{
	remove(TEST_FILENAME);
}

void test_texture_load_pixels_detects_toctou_dimension_mismatch(void)
{
	int w, h, c;

	// Enable TOCTOU simulation: info returns 10x10, load returns 9000x9000
	mock_stbi_set_toctou_simulation(1);

	// This should return NULL because dimensions change between info and
	// load
	float* data = texture_load_pixels(TEST_FILENAME, &w, &h, &c);

	if (data != NULL) {
		stbi_image_free(data);
	}

	TEST_ASSERT_NULL_MESSAGE(
	    data,
	    "texture_load_pixels should return NULL when dimensions exceed MAX "
	    "after load");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_load_pixels_detects_toctou_dimension_mismatch);
	return UNITY_END();
}
