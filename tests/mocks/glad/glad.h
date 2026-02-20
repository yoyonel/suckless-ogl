#ifndef __glad_h_
#define __glad_h_

#include <stddef.h>

typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef long GLsizeiptr;
typedef long GLintptr;
typedef unsigned char GLubyte;

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STREAM_DRAW 0x88E0
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_UNSIGNED_INT 0x1405
#define GL_CULL_FACE 0x0B44
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_LINE_LOOP 0x0002
#define GL_LINES 0x0001
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPUTE_SHADER 0x91B9
#define GL_PROGRAM 0x82E2
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH 0x8B87
#define GL_PROGRAM_BINARY_LENGTH 0x8741
#define APIENTRY
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA16F 0x881A
#define GL_RGBA 0x1908
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_LINEAR 0x2601
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_NO_ERROR 0
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_TIMESTAMP 0x8E28
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DONT_CARE 0x1100
#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x8249
#define GL_DEBUG_SOURCE_APPLICATION 0x824A
#define GL_DEBUG_SOURCE_OTHER 0x824B
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP 0x8269
#define GL_DEBUG_TYPE_POP_GROUP 0x826A
#define GL_DEBUG_TYPE_OTHER 0x8251
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_CONTEXT_FLAGS 0x821E
#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_TEXTURE0 0x84C0
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008

typedef void(APIENTRY* GLDEBUGPROC)(GLenum source, GLenum type, GLuint id,
                                    GLenum severity, GLsizei length,
                                    const GLchar* message,
                                    const void* userParam);

GLuint glCreateShader(GLenum type);
void glShaderSource(GLuint shader, GLsizei count, const GLchar** string,
                    const GLint* length);
void glCompileShader(GLuint shader);
void glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length,
                        GLchar* infoLog);
void glDeleteShader(GLuint shader);
GLuint glCreateProgram(void);
void glAttachShader(GLuint program, GLuint shader);
void glLinkProgram(GLuint program);
void glGetProgramiv(GLuint program, GLenum pname, GLint* params);
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length,
                         GLchar* infoLog);
void glDeleteProgram(GLuint program);
void glObjectLabel(GLenum identifier, GLuint name, GLsizei length,
                   const GLchar* label);
void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize,
                        GLsizei* length, GLint* size, GLenum* type,
                        GLchar* name);
GLint glGetUniformLocation(GLuint program, const GLchar* name);
void glUseProgram(GLuint program);
void glUniform1i(GLint location, GLint v0);
void glUniform1f(GLint location, float v0);
void glUniform2fv(GLint location, GLsizei count, const float* value);
void glUniform3fv(GLint location, GLsizei count, const float* value);
void glUniform4fv(GLint location, GLsizei count, const float* value);
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                        const float* value);
void glPopDebugGroup(void);
void glDeleteTextures(GLsizei n, const GLuint* textures);

void glGenTextures(GLsizei n, GLuint* textures);
void glBindTexture(GLenum target, GLuint texture);
void glPixelStorei(GLenum pname, GLint param);
void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                    GLsizei width, GLsizei height);
void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLint border, GLenum format,
                  GLenum type, const void* pixels);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const void* pixels);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glGenerateMipmap(GLenum target);
GLenum glGetError(void);
void glGetIntegerv(GLenum pname, GLint* data);
void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                              GLint* params);

void glGenBuffers(GLsizei n, GLuint* buffers);
void glBindBuffer(GLenum target, GLuint buffer);
void glBufferData(GLenum target, GLsizeiptr size, const void* data,
                  GLenum usage);
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                     const void* data);
void glDeleteBuffers(GLsizei n, const GLuint* buffers);
void* glMapBuffer(GLenum target, GLenum access);
void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                       GLenum access);
GLboolean glUnmapBuffer(GLenum target);

void glGenVertexArrays(GLsizei n, GLuint* arrays);
void glBindVertexArray(GLuint array);
void glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
void glEnableVertexAttribArray(GLuint index);
void glDisableVertexAttribArray(GLuint index);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                           GLboolean normalized, GLsizei stride,
                           const void* pointer);
void glVertexAttribDivisor(GLuint index, GLuint divisor);

void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei instancecount);
void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                             const void* indices, GLsizei instancecount);
void glEnable(GLenum cap);
void glDisable(GLenum cap);
GLboolean glIsEnabled(GLenum cap);
void glDebugMessageCallback(GLDEBUGPROC callback, const void* userParam);
void glDebugMessageControl(GLenum source, GLenum type, GLenum severity,
                           GLsizei count, const GLuint* ids, GLboolean enabled);

const GLubyte* glGetString(GLenum name);
void glActiveTexture(GLenum texture);

#include <stdint.h>

typedef uint64_t GLuint64;
typedef int64_t GLint64;

void glGenQueries(GLsizei n, GLuint* ids);
void glDeleteQueries(GLsizei n, const GLuint* ids);
void glQueryCounter(GLuint id, GLenum target);
void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params);
void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params);

void glFlush(void);
void glFinish(void);

#endif
