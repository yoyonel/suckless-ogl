/**
 * @file mock_gl_standalone.c
 * @brief Implementation of a minimal mock OpenGL environment.
 */

#include "mock_gl_standalone.h"

#include <stdio.h>
#include <string.h>

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

void glActiveTexture(GLenum texture)
{
	(void)texture;
}
void glGenTextures(GLsizei n, GLuint* textures)
{
	for (int i = 0; i < n; i++)
		if (textures)
			textures[i] = (GLuint)(i + 1);
}
void glDeleteTextures(GLsizei n, const GLuint* textures)
{
	(void)n;
	(void)textures;
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
void glTexImage3D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                  GLenum format, GLenum type, const void* pixels)
{
	(void)target;
	(void)level;
	(void)internalformat;
	(void)width;
	(void)height;
	(void)depth;
	(void)border;
	(void)format;
	(void)type;
	(void)pixels;
}
void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLint zoffset, GLsizei width, GLsizei height,
                     GLsizei depth, GLenum format, GLenum type,
                     const void* pixels)
{
	(void)target;
	(void)level;
	(void)xoffset;
	(void)yoffset;
	(void)zoffset;
	(void)width;
	(void)height;
	(void)depth;
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
const GLchar* glGetString(GLenum name)
{
	(void)name;
	return "Mock GL";
}
void glGetIntegerv(GLenum pname, GLint* data)
{
	if (data) {
		if (pname == GL_POLYGON_MODE) {
			data[0] = GL_FILL;
			data[1] = GL_FILL;
		} else {
			*data = 0;
		}
	}
}
void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                              GLint* params)
{
	(void)target;
	(void)level;
	(void)pname;
	if (params)
		*params = 64;
}

void glGenBuffers(GLsizei n, GLuint* buffers)
{
	(void)n;
	if (buffers)
		*buffers = g_generated_buffer_id;
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
	if (buffers) {
		g_last_deleted_buffer = *buffers;
		g_delete_buffer_call_count++;
	}
	(void)n;
}
void glBufferStorage(GLenum target, GLsizeiptr size, const void* data,
                     GLbitfield flags)
{
	(void)target;
	(void)size;
	(void)data;
	(void)flags;
}
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	(void)target;
	(void)index;
	(void)buffer;
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
void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                        void* data)
{
	(void)target;
	(void)offset;
	(void)size;
	if (data)
		*(float*)data = 1.0f;
}
void* glMapBuffer(GLenum target, GLenum access)
{
	(void)target;
	(void)access;
	static char buf[1024];
	return buf;
}
void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                       GLbitfield access)
{
	(void)target;
	(void)offset;
	(void)length;
	(void)access;
	static char buf[1024];
	return buf;
}
GLboolean glUnmapBuffer(GLenum target)
{
	(void)target;
	return GL_TRUE;
}

void glGenVertexArrays(GLsizei n, GLuint* arrays)
{
	(void)n;
	if (arrays)
		*arrays = DEFAULT_VAO_ID;
}
void glDeleteVertexArrays(GLsizei n, const GLuint* arrays)
{
	(void)n;
	(void)arrays;
}
void glBindVertexArray(GLuint array)
{
	(void)array;
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
void glVertexAttrib4fv(GLuint index, const GLfloat* v)
{
	(void)index;
	(void)v;
}
void glVertexAttribDivisor(GLuint index, GLuint divisor)
{
	(void)index;
	(void)divisor;
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	(void)mode;
	(void)first;
	(void)count;
}
void glDrawElements(GLenum mode, GLsizei count, GLenum type,
                    const void* indices)
{
	(void)mode;
	(void)count;
	(void)type;
	(void)indices;
}
void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei instancecount)
{
	(void)mode;
	(void)first;
	(void)count;
	(void)instancecount;
}
void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                             const void* indices, GLsizei instancecount)
{
	(void)mode;
	(void)count;
	(void)type;
	(void)indices;
	(void)instancecount;
}

uint32_t glCreateShader(GLenum type)
{
	(void)type;
	return 1;
}
void glShaderSource(GLuint shader, GLsizei count, const char* const* string,
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
	if (length)
		*length = 0;
	if (infoLog)
		infoLog[0] = '\0';
}
void glDeleteShader(GLuint shader)
{
	(void)shader;
}

uint32_t glCreateProgram(void)
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
	if (length)
		*length = 0;
	if (infoLog)
		infoLog[0] = '\0';
}
void glUseProgram(GLuint program)
{
	(void)program;
}
void glDeleteProgram(GLuint program)
{
	(void)program;
}

GLint glGetUniformLocation(GLuint program, const GLchar* name)
{
	(void)program;
	(void)name;
	return 0;
}
void glUniform1i(GLint location, GLint v0)
{
	(void)location;
	(void)v0;
}
void glUniform1iv(GLint location, GLsizei count, const GLint* value)
{
	(void)location;
	(void)count;
	(void)value;
}
void glUniform1f(GLint location, GLfloat v0)
{
	(void)location;
	(void)v0;
}
void glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
	(void)location;
	(void)v0;
	(void)v1;
}
void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
	(void)location;
	(void)v0;
	(void)v1;
	(void)v2;
}
void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	(void)location;
	(void)v0;
	(void)v1;
	(void)v2;
	(void)v3;
}
void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)
{
	(void)location;
	(void)v0;
	(void)v1;
	(void)v2;
}
void glUniform1ui(GLint location, GLuint v0)
{
	(void)location;
	(void)v0;
}
void glUniform2fv(GLint location, GLsizei count, const GLfloat* value)
{
	(void)location;
	(void)count;
	(void)value;
}
void glUniform3fv(GLint location, GLsizei count, const GLfloat* value)
{
	(void)location;
	(void)count;
	(void)value;
}
void glUniform4fv(GLint location, GLsizei count, const GLfloat* value)
{
	(void)location;
	(void)count;
	(void)value;
}
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat* value)
{
	(void)location;
	(void)count;
	(void)transpose;
	(void)value;
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
GLenum glCheckFramebufferStatus(GLenum target)
{
	(void)target;
	return GL_FRAMEBUFFER_COMPLETE;
}
void glClear(GLbitfield mask)
{
	(void)mask;
}
void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	(void)red;
	(void)green;
	(void)blue;
	(void)alpha;
}
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	(void)x;
	(void)y;
	(void)width;
	(void)height;
}

void glGenQueries(GLsizei n, GLuint* ids)
{
	for (GLsizei i = 0; i < n; i++)
		if (ids)
			ids[i] = (GLuint)(i + 1);
}
void glDeleteQueries(GLsizei n, const GLuint* ids)
{
	(void)n;
	(void)ids;
}
void glQueryCounter(GLuint id, GLenum target)
{
	(void)id;
	(void)target;
}
void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params)
{
	(void)id;
	(void)pname;
	if (params)
		*params = 1000;
}
void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params)
{
	(void)id;
	(void)pname;
	if (params)
		*params = GL_TRUE;
}

void glFlush(void)
{
}
void glFinish(void)
{
}

void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                       GLuint num_groups_z)
{
	(void)num_groups_x;
	(void)num_groups_y;
	(void)num_groups_z;
}
void glMemoryBarrier(GLbitfield barriers)
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
	if (length)
		*length = 0;
	if (size)
		*size = 1;
	if (type)
		*type = GL_FLOAT;
	if (name)
		name[0] = '\0';
}

GLsync glFenceSync(GLenum condition, GLbitfield flags)
{
	(void)condition;
	(void)flags;
	static int s;
	return (GLsync)&s;
}
GLenum glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
	(void)sync;
	(void)flags;
	(void)timeout;
	return GL_CONDITION_SATISFIED;
}
void glDeleteSync(GLsync sync)
{
	(void)sync;
}

void gl_debug_push_group(const char* name)
{
	(void)name;
}

void gl_debug_pop_group(void)
{
}

void glDepthMask(GLboolean flag)
{
	(void)flag;
}
void glEnablei(GLenum target, GLuint index)
{
	(void)target;
	(void)index;
}
void glDisablei(GLenum target, GLuint index)
{
	(void)target;
	(void)index;
}
void glPolygonOffset(GLfloat factor, GLfloat units)
{
	(void)factor;
	(void)units;
}
void glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass)
{
	(void)sfail;
	(void)dpfail;
	(void)dppass;
}
void glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	(void)func;
	(void)ref;
	(void)mask;
}
void glStencilMask(GLuint mask)
{
	(void)mask;
}

void glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b,
                  GLboolean a)
{
	(void)index;
	(void)r;
	(void)g;
	(void)b;
	(void)a;
}
