/**
 * @file mock_gl_standalone.c
 * @brief Implementation of a minimal mock OpenGL environment.
 *
 * NOTE: This file includes "mock_gl_standalone.h", which includes
 * "glad/glad.h". When compiled for standalone tests (e.g.
 * test_ibl_coordinator), the "glad/glad.h" resolved is the MANUAL mock in
 * "tests/mocks/glad/glad.h". This manual mock defines GL functions as regular C
 * functions (not macros). Thus, implementing them here causes no conflict.
 *
 * However, if this file were compiled in an environment where "glad.h" is the
 * REAL one (defining macros expanding to function pointers), this would cause a
 * build error. Ensure CMake configuration for standalone tests strictly
 * controls include paths to favor "tests/mocks" over
 * "build/_deps/glad-src/include".
 */

#include "mock_gl_standalone.h"

#include <stdio.h> /* For definition of NULL if needed, though not used here */

/* Control Variables */
static GLuint g_generated_buffer_id = DEFAULT_BUFFER_ID;
static GLuint g_last_deleted_buffer = 0;
static int g_delete_buffer_call_count = 0;
static int g_buffer_data_call_count = 0;
static int g_buffer_sub_data_call_count = 0;
static GLsizeiptr g_last_buffer_data_size = 0;
static GLsizeiptr g_last_buffer_sub_data_size = 0;

void mock_gl_reset_calls(void)
{
	g_generated_buffer_id = DEFAULT_BUFFER_ID;
	g_last_deleted_buffer = 0;
	g_delete_buffer_call_count = 0;
	g_buffer_data_call_count = 0;
	g_buffer_sub_data_call_count = 0;
	g_last_buffer_data_size = 0;
	g_last_buffer_sub_data_size = 0;
}

GLuint mock_gl_get_generated_buffer_id(void)
{
	return g_generated_buffer_id;
}

GLuint mock_gl_get_generated_vao_id(void)
{
	return DEFAULT_VAO_ID;
}

GLuint mock_gl_get_last_deleted_buffer(void)
{
	return g_last_deleted_buffer;
}

int mock_gl_get_delete_buffer_call_count(void)
{
	return g_delete_buffer_call_count;
}

int mock_gl_get_buffer_data_call_count(void)
{
	return g_buffer_data_call_count;
}

int mock_gl_get_buffer_sub_data_call_count(void)
{
	return g_buffer_sub_data_call_count;
}

GLsizeiptr mock_gl_get_last_buffer_data_size(void)
{
	return g_last_buffer_data_size;
}

GLsizeiptr mock_gl_get_last_buffer_sub_data_size(void)
{
	return g_last_buffer_sub_data_size;
}

/* -------------------------------------------------------------------------- */
/*                            MOCK IMPLEMENTATIONS                            */
/* -------------------------------------------------------------------------- */

void glGenBuffers(GLsizei n, GLuint* buffers)
{
	(void)n;
	if (buffers) {
		*buffers = g_generated_buffer_id;
	}
}

void glBindBuffer(GLenum target, GLuint buffer)
{
	(void)target;
	(void)buffer;
}

void glBufferData(GLenum target, GLsizeiptr size, const void* data,
                  GLenum usage)
{
	(void)target;
	(void)data;
	(void)usage;
	g_buffer_data_call_count++;
	g_last_buffer_data_size = size;
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                     const void* data)
{
	(void)target;
	(void)offset;
	(void)data;
	g_buffer_sub_data_call_count++;
	g_last_buffer_sub_data_size = size;
}

void glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
	(void)n;
	if (buffers) {
		g_last_deleted_buffer = *buffers;
		g_delete_buffer_call_count++;
	}
}

void glGenVertexArrays(GLsizei n, GLuint* arrays)
{
	(void)n;
	if (arrays) {
		*arrays = DEFAULT_VAO_ID;
	}
}

void glBindVertexArray(GLuint array)
{
	(void)array;
}

void glDeleteVertexArrays(GLsizei n, const GLuint* arrays)
{
	(void)n;
	(void)arrays;
}

void glEnableVertexAttribArray(GLuint index)
{
	(void)index;
}

void glDisableVertexAttribArray(GLuint index)
{
	(void)index;
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                           GLboolean normalized, GLsizei stride,
                           const void* pointer)
{
	(void)index;
	(void)size;
	(void)type;
	(void)normalized;
	(void)stride;
	(void)pointer;
}

void glVertexAttribDivisor(GLuint index, GLuint divisor)
{
	(void)index;
	(void)divisor;
}

void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei instancecount)
{
	(void)mode;
	(void)first;
	(void)count;
	(void)instancecount;
}

void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                         GLintptr readOffset, GLintptr writeOffset,
                         GLsizeiptr size)
{
	(void)readTarget;
	(void)writeTarget;
	(void)readOffset;
	(void)writeOffset;
	(void)size;
}

void glEnable(GLenum cap)
{
	(void)cap;
}

void glDisable(GLenum cap)
{
	(void)cap;
}

GLboolean glIsEnabled(GLenum cap)
{
	(void)cap;
	return GL_FALSE;
}

/* Texture Mocks (needed for test_texture_toctou) */
void glGenTextures(GLsizei n, GLuint* textures)
{
	(void)n;
	if (textures) {
		*textures = 1;
	}
}

void glBindTexture(GLenum target, GLuint texture)
{
	(void)target;
	(void)texture;
}

void glPixelStorei(GLenum pname, GLint param)
{
	(void)pname;
	(void)param;
}

void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                    GLsizei width, GLsizei height)
{
	(void)target;
	(void)levels;
	(void)internalformat;
	(void)width;
	(void)height;
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLint border, GLenum format,
                  GLenum type, const void* pixels)
{
	(void)target;
	(void)level;
	(void)internalformat;
	(void)width;
	(void)height;
	(void)border;
	(void)format;
	(void)type;
	(void)pixels;
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const void* pixels)
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

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	(void)target;
	(void)pname;
	(void)param;
}

void glGenerateMipmap(GLenum target)
{
	(void)target;
}

GLenum glGetError(void)
{
	return 0;
}

void glDeleteTextures(GLsizei n, const GLuint* textures)
{
	(void)n;
	(void)textures;
}

/* -------------------------------------------------------------------------- */
/*                          SHADER MOCK IMPLEMENTATIONS                       */
/* -------------------------------------------------------------------------- */

GLuint glCreateShader(GLenum type)
{
	(void)type;
	return 1;
}
void glShaderSource(GLuint shader, GLsizei count, const GLchar** string,
                    const GLint* length)
{
	(void)shader;
	(void)count;
	(void)string;
	(void)length;
}
void glCompileShader(GLuint shader)
{
	(void)shader;
}
void glGetShaderiv(GLuint shader, GLenum pname, GLint* params)
{
	(void)shader;
	(void)pname;
	if (params)
		*params = GL_TRUE;
}
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length,
                        GLchar* infoLog)
{
	(void)shader;
	(void)bufSize;
	(void)length;
	(void)infoLog;
}
void glDeleteShader(GLuint shader)
{
	(void)shader;
}
GLuint glCreateProgram(void)
{
	return 1;
}
void glAttachShader(GLuint program, GLuint shader)
{
	(void)program;
	(void)shader;
}
void glLinkProgram(GLuint program)
{
	(void)program;
}
void glGetProgramiv(GLuint program, GLenum pname, GLint* params)
{
	(void)program;
	(void)pname;
	if (params)
		*params = GL_TRUE;
}
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length,
                         GLchar* infoLog)
{
	(void)program;
	(void)bufSize;
	(void)length;
	(void)infoLog;
}
void glDeleteProgram(GLuint program)
{
	(void)program;
}
void glObjectLabel(GLenum identifier, GLuint name, GLsizei length,
                   const GLchar* label)
{
	(void)identifier;
	(void)name;
	(void)length;
	(void)label;
}
void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize,
                        GLsizei* length, GLint* size, GLenum* type,
                        GLchar* name)
{
	(void)program;
	(void)index;
	(void)bufSize;
	(void)length;
	(void)size;
	(void)type;
	(void)name;
}
GLint glGetUniformLocation(GLuint program, const GLchar* name)
{
	(void)program;
	(void)name;
	return 0;
}
void glUseProgram(GLuint program)
{
	(void)program;
}
void glUniform1i(GLint location, GLint v0)
{
	(void)location;
	(void)v0;
}
void glUniform1f(GLint location, float v0)
{
	(void)location;
	(void)v0;
}
void glUniform2fv(GLint location, GLsizei count, const float* value)
{
	(void)location;
	(void)count;
	(void)value;
}
void glUniform3fv(GLint location, GLsizei count, const float* value)
{
	(void)location;
	(void)count;
	(void)value;
}
void glUniform4fv(GLint location, GLsizei count, const float* value)
{
	(void)location;
	(void)count;
	(void)value;
}
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                        const float* value)
{
	(void)location;
	(void)count;
	(void)transpose;
	(void)value;
}
void glPushDebugGroup(GLenum source, GLuint id, GLsizei length,
                      const GLchar* message)
{
	(void)source;
	(void)id;
	(void)length;
	(void)message;
}
void glPopDebugGroup(void)
{
}

GLenum glCheckFramebufferStatus(GLenum target)
{
	(void)target;
	return GL_FRAMEBUFFER_COMPLETE;
}

void glGetIntegerv(GLenum pname, GLint* data)
{
	(void)pname;
	if (data) {
		if (pname == GL_POLYGON_MODE) {
			data[0] = GL_FILL;
			data[1] = GL_FILL;
		} else {
			*data = 0;
		}
	}
}

void glPolygonMode(GLenum face, GLenum mode)
{
	(void)face;
	(void)mode;
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	(void)sfactor;
	(void)dfactor;
}

void glActiveTexture(GLenum texture)
{
	(void)texture;
}

void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                              GLint* params)
{
	(void)target;
	(void)level;
	(void)pname;
	(void)params;
}

void* glMapBuffer(GLenum target, GLenum access)
{
	(void)target;
	(void)access;
	/* Return a dummy pointer that is non-NULL */
	static char dummy_buffer[1024];
	return dummy_buffer;
}

GLboolean glUnmapBuffer(GLenum target)
{
	(void)target;
	return GL_TRUE;
}

void glGenQueries(GLsizei n, GLuint* ids)
{
	for (GLsizei i = 0; i < n; i++) {
		if (ids) {
			ids[i] = (GLuint)(i + 1); /* Dummy unique IDs */
		}
	}
}

void glDeleteQueries(GLsizei n, const GLuint* ids)
{
	(void)n;
	(void)ids;
}

void glQueryCounter(GLuint query_id, GLenum target)
{
	(void)query_id;
	(void)target;
}

void glGetQueryObjectui64v(GLuint query_id, GLenum pname, GLuint64* params)
{
	(void)query_id;
	(void)pname;
	if (params) {
		*params = 1000; /* Dummy timestamp value */
	}
}

void glGetQueryObjectiv(GLuint query_id, GLenum pname, GLint* params)
{
	(void)query_id;
	(void)pname;
	if (params) {
		*params = GL_TRUE; /* Available */
	}
}

void glFlush(void)
{
}

void glFinish(void)
{
}

const GLchar* glGetString(GLenum name)
{
	if (name == GL_RENDERER) {
		return "Mock Renderer (Standalone)";
	}
	return "Mock GL";
}

void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                       GLuint num_groups_z)
{
	(void)num_groups_x;
	(void)num_groups_y;
	(void)num_groups_z;
}

void glMemoryBarrier(unsigned int barriers)
{
	(void)barriers;
}

void glBindImageTexture(GLuint unit, GLuint texture, GLint level,
                        GLboolean layered, GLint layer, GLenum access,
                        GLenum format)
{
	(void)unit;
	(void)texture;
	(void)level;
	(void)layered;
	(void)layer;
	(void)access;
	(void)format;
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	(void)target;
	(void)index;
	(void)buffer;
}

void glBufferStorage(GLenum target, GLsizeiptr size, const void* data,
                     unsigned int flags)
{
	(void)target;
	(void)size;
	(void)data;
	(void)flags;
}

void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                        void* data)
{
	(void)target;
	(void)offset;
	(void)size;
	if (data) {
		/* Return a dummy non-zero mean luminance */
		*(float*)data = 1.0F;
	}
}

void glUniform1ui(GLint location, GLuint v0)
{
	(void)location;
	(void)v0;
}
