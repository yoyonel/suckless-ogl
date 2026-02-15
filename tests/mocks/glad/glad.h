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

#define GL_ARRAY_BUFFER 0
#define GL_ELEMENT_ARRAY_BUFFER 0
#define GL_DYNAMIC_DRAW 0
#define GL_FLOAT 0
#define GL_UNSIGNED_INT 0
#define GL_CULL_FACE 0
#define GL_TRIANGLES 0
#define GL_TRIANGLE_STRIP 0
#define GL_LINE_LOOP 0
#define GL_LINES 0
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_COMPILE_STATUS 0
#define GL_LINK_STATUS 0
#define GL_INFO_LOG_LENGTH 0
#define GL_VERTEX_SHADER 0
#define GL_FRAGMENT_SHADER 0
#define GL_COMPUTE_SHADER 0
#define GL_PROGRAM 0
#define GL_ACTIVE_UNIFORMS 0
#define GL_ACTIVE_UNIFORM_MAX_LENGTH 0
#define GL_PROGRAM_BINARY_LENGTH 0
#define APIENTRY
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA16F 0x881A
#define GL_RGBA 0x1908
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_NO_ERROR 0
#define GL_TEXTURE0 0x84C0
#define GL_VERTEX_ARRAY 0x8074
#define GL_STATIC_DRAW 0x88E4
#define GL_BUFFER 0x82E0
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_TEXTURE 0x1702

typedef unsigned char GLubyte;

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
void glActiveTexture(GLenum texture);

void glGenTextures(GLsizei n, GLuint* textures);
void glBindTexture(GLenum target, GLuint texture);
void glPixelStorei(GLenum pname, GLint param);
void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                    GLsizei width, GLsizei height);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const void* pixels);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glGenerateMipmap(GLenum target);
GLenum glGetError(void);

void glGenBuffers(GLsizei n, GLuint* buffers);
void glBindBuffer(GLenum target, GLuint buffer);
void glBufferData(GLenum target, GLsizeiptr size, const void* data,
                  GLenum usage);
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                     const void* data);
void glDeleteBuffers(GLsizei n, const GLuint* buffers);

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
GLenum glCheckFramebufferStatus(GLenum target);
const GLubyte* glGetString(GLenum name);

#endif
