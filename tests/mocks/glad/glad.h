#ifndef __glad_h_
#define __glad_h_

#include <stddef.h>
#include <stdint.h>

typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef long GLsizeiptr;
typedef long GLintptr;
typedef unsigned int GLbitfield;
typedef uint64_t GLuint64;
typedef int64_t GLint64;
typedef struct __GLsync* GLsync;

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STATIC_DRAW 0x88E4
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
#define GL_TEXTURE_3D 0x806F
#define GL_RGBA16F 0x881A
#define GL_RG16F 0x822F
#define GL_RGBA 0x1908
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_NO_ERROR 0
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_TIMESTAMP 0x8E28
#define GL_RENDERER 0x1F01
#define GL_VENDOR 0x1F00
#define GL_VERSION 0x1F02
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#define GL_TEXTURE_FETCH_BARRIER_BIT 0x00000008
#define GL_ALL_BARRIER_BITS 0xFFFFFFFF
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020
#define GL_CLIENT_STORAGE_BIT 0x0200
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_DYNAMIC_COPY 0x88EA
#define GL_COPY_READ_BUFFER 0x8F36
#define GL_COPY_WRITE_BUFFER 0x8F37
#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT 0x00000001
#define GL_BUFFER_UPDATE_BARRIER_BIT 0x00000200
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE8 0x84C8
#define GL_TEXTURE9 0x84C9
#define GL_TEXTURE10 0x84CA
#define GL_TEXTURE11 0x84CB
#define GL_TEXTURE12 0x84CC
#define GL_TEXTURE13 0x84CD
#define GL_TEXTURE14 0x84CE

#define GL_DEBUG_SOURCE_APPLICATION 0x8246
#define GL_BUFFER 0x82E0
#define GL_VERTEX_ARRAY 0x8074
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE 0x1702
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_POLYGON_MODE 0x0B40
#define GL_FRONT_AND_BACK 0x0408
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FILL 0x1B02
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_STREAM_DRAW 0x88E0
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003

#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#define GL_ALREADY_SIGNALED 0x911A
#define GL_TIMEOUT_EXPIRED 0x911B
#define GL_CONDITION_SATISFIED 0x911C
#define GL_WAIT_FAILED 0x911D

void glActiveTexture(GLenum texture);
void glGenTextures(GLsizei n, GLuint* textures);
void glDeleteTextures(GLsizei n, const GLuint* textures);
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
void glTexImage3D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                  GLenum format, GLenum type, const void* pixels);
void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLint zoffset, GLsizei width, GLsizei height,
                     GLsizei depth, GLenum format, GLenum type,
                     const void* pixels);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glGenerateMipmap(GLenum target);
GLenum glGetError(void);
const GLchar* glGetString(GLenum name);
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
void glBufferStorage(GLenum target, GLsizeiptr size, const void* data,
                     GLbitfield flags);
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                         GLintptr readOffset, GLintptr writeOffset,
                         GLsizeiptr size);
void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                        void* data);
void* glMapBuffer(GLenum target, GLenum access);
void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                       GLbitfield access);
GLboolean glUnmapBuffer(GLenum target);

void glGenVertexArrays(GLsizei n, GLuint* arrays);
void glBindVertexArray(GLuint array);
void glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
void glEnableVertexAttribArray(GLuint index);
void glDisableVertexAttribArray(GLuint index);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type,
                           GLboolean normalized, GLsizei stride,
                           const void* pointer);
void glVertexAttrib4fv(GLuint index, const GLfloat* v);
void glVertexAttribDivisor(GLuint index, GLuint divisor);

void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glDrawElements(GLenum mode, GLsizei count, GLenum type,
                    const void* indices);
void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei instancecount);
void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                             const void* indices, GLsizei instancecount);

uint32_t glCreateShader(GLenum type);
void glShaderSource(GLuint shader, GLsizei count, const char* const* string,
                    const GLint* length);
void glCompileShader(GLuint shader);
void glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length,
                        GLchar* infoLog);
void glDeleteShader(GLuint shader);

uint32_t glCreateProgram(void);
void glAttachShader(GLuint program, GLuint shader);
void glLinkProgram(GLuint program);
void glGetProgramiv(GLuint program, GLenum pname, GLint* params);
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length,
                         GLchar* infoLog);
void glUseProgram(GLuint program);
void glDeleteProgram(GLuint program);

GLint glGetUniformLocation(GLuint program, const GLchar* name);
void glUniform1i(GLint location, GLint v0);
void glUniform1iv(GLint location, GLsizei count, const GLint* value);
void glUniform1f(GLint location, GLfloat v0);
void glUniform2f(GLint location, GLfloat v0, GLfloat v1);
void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
                 GLfloat v3);
void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
void glUniform1ui(GLint location, GLuint v0);
void glUniform2fv(GLint location, GLsizei count, const GLfloat* value);
void glUniform3fv(GLint location, GLsizei count, const GLfloat* value);
void glUniform4fv(GLint location, GLsizei count, const GLfloat* value);
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat* value);

void glEnable(GLenum cap);
void glDisable(GLenum cap);
GLboolean glIsEnabled(GLenum cap);
void glPolygonMode(GLenum face, GLenum mode);
void glBlendFunc(GLenum sfactor, GLenum dfactor);
GLenum glCheckFramebufferStatus(GLenum target);
void glClear(GLbitfield mask);
void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

void glGenQueries(GLsizei n, GLuint* ids);
void glDeleteQueries(GLsizei n, const GLuint* ids);
void glQueryCounter(GLuint id, GLenum target);
void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params);
void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params);
void glFlush(void);
void glFinish(void);

void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                       GLuint num_groups_z);
void glMemoryBarrier(GLbitfield barriers);
void glBindImageTexture(GLuint unit, GLuint texture, GLint level,
                        GLboolean layered, GLint layer, GLenum access,
                        GLenum format);

void glPushDebugGroup(GLenum source, GLuint id, GLsizei length,
                      const GLchar* message);
void glPopDebugGroup(void);
void glObjectLabel(GLenum identifier, GLuint name, GLsizei length,
                   const GLchar* label);
void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize,
                        GLsizei* length, GLint* size, GLenum* type,
                        GLchar* name);

GLsync glFenceSync(GLenum condition, GLbitfield flags);
GLenum glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
void glDeleteSync(GLsync sync);

#endif
