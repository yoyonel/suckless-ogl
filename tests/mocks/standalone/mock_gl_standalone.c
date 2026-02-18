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

void glGetIntegerv(GLenum pname, GLint* data)
{
	if (pname == GL_CONTEXT_FLAGS && data) {
		*data = GL_CONTEXT_FLAG_DEBUG_BIT;
	}
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
void glPopDebugGroup(void)
{
}

void glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam)
{
	(void)callback;
	(void)userParam;
}

void glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled)
{
	(void)source;
	(void)type;
	(void)severity;
	(void)count;
	(void)ids;
	(void)enabled;
}
