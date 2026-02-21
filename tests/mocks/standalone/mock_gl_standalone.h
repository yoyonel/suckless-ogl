#ifndef MOCK_GL_STANDALONE_H
#define MOCK_GL_STANDALONE_H

#include <stdint.h>

/* Define types compatible with GLAD/OpenGL */
typedef uint64_t GLuint64;
typedef int64_t GLint64;
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef long GLsizeiptr;
typedef long GLintptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_RGBA 0x1908
#define GL_RGBA16F 0x881A
#define GL_RG16F 0x822F
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_WRITE_ONLY 0x88B9
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_TIMESTAMP 0x8E28
#define GL_RENDERER 0x1F01
#define GL_TEXTURE0 0x84C0
#define GL_DEBUG_SOURCE_APPLICATION 0x8246
#define GL_BUFFER 0x82E0
#define GL_VERTEX_ARRAY 0x8074
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_VENDOR 0x1F00
#define GL_VERSION 0x1F02
#define GL_TEXTURE 0x1702
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_POLYGON_MODE 0x0B40
#define GL_FRONT_AND_BACK 0x0408
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FILL 0x1B02
#define GL_NEAREST 0x2600

/* Defaults */
#define DEFAULT_BUFFER_ID 123
#define DEFAULT_VAO_ID 456

/* Shader & Program APIs */
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
void glPushDebugGroup(GLenum source, GLuint id, GLsizei length,
                      const GLchar* message);
void glPopDebugGroup(void);
GLenum glCheckFramebufferStatus(GLenum target);
void glGetIntegerv(GLenum pname, GLint* data);
void glPolygonMode(GLenum face, GLenum mode);
void glBlendFunc(GLenum sfactor, GLenum dfactor);
void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                              GLint* params);
void glActiveTexture(GLenum texture);
void* glMapBuffer(GLenum target, GLenum access);
GLboolean glUnmapBuffer(GLenum target);
void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLint border, GLenum format,
                  GLenum type, const void* pixels);
void glGenQueries(GLsizei n, GLuint* ids);
void glDeleteQueries(GLsizei n, const GLuint* ids);
void glQueryCounter(GLuint id, GLenum target);
void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params);
void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params);
void glFlush(void);
void glFinish(void);

const GLchar* glGetString(GLenum name);
void glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                       GLuint num_groups_z);
void glMemoryBarrier(unsigned int barriers);
void glBindImageTexture(GLuint unit, GLuint texture, GLint level,
                        GLboolean layered, GLint layer, GLenum access,
                        GLenum format);
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void glBufferStorage(GLenum target, GLsizeiptr size, const void* data,
                     unsigned int flags);
void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                        void* data);
void glUniform1ui(GLint location, GLuint v0);

/* Control API */
void mock_gl_reset_calls(void);
GLuint mock_gl_get_generated_buffer_id(void);
GLuint mock_gl_get_generated_vao_id(void);
GLuint mock_gl_get_last_deleted_buffer(void);
int mock_gl_get_delete_buffer_call_count(void);
int mock_gl_get_buffer_data_call_count(void);
int mock_gl_get_buffer_sub_data_call_count(void);
GLsizeiptr mock_gl_get_last_buffer_data_size(void);
GLsizeiptr mock_gl_get_last_buffer_sub_data_size(void);

#endif /* MOCK_GL_STANDALONE_H */
