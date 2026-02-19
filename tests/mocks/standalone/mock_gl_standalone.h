#ifndef MOCK_GL_STANDALONE_H
#define MOCK_GL_STANDALONE_H

#include <stddef.h>
#include <stdint.h>

/* Define types compatible with GLAD/OpenGL */
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef char GLchar;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef long GLsizeiptr;
typedef long GLintptr;
typedef uint64_t GLuint64;

#define GL_FALSE 0
#define GL_TRUE 1

/* Defaults */
#define DEFAULT_BUFFER_ID 123
#define DEFAULT_VAO_ID 456
#define DEFAULT_QUERY_ID 789

/* Enums */
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DONT_CARE 0x1100
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_TIMESTAMP 0x8E28
#define GL_CONTEXT_FLAGS 0x821E
#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002

/* Debug Source */
#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x8249
#define GL_DEBUG_SOURCE_APPLICATION 0x824A
#define GL_DEBUG_SOURCE_OTHER 0x824B

/* Debug Type */
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_OTHER 0x8251
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP 0x8269
#define GL_DEBUG_TYPE_POP_GROUP 0x826A

/* Debug Severity */
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B

/* Debug Callback Type */
typedef void (*GLDEBUGPROC)(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar* message, const void* userParam);

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
void glPopDebugGroup(void);
void glGetIntegerv(GLenum pname, GLint* data);

/* Query APIs */
void glGenQueries(GLsizei n, GLuint* ids);
void glDeleteQueries(GLsizei n, const GLuint* ids);
void glQueryCounter(GLuint id, GLenum target);
void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params);
void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params);
void glFlush(void);
void glFinish(void);

/* Debug APIs */
void glDebugMessageCallback(GLDEBUGPROC callback, const void* userParam);
void glDebugMessageControl(GLenum source, GLenum type, GLenum severity,
                           GLsizei count, const GLuint* ids, GLboolean enabled);
void glEnable(GLenum cap);

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

/* Query specific mocks */
int mock_gl_get_gen_queries_call_count(void);
int mock_gl_get_delete_queries_call_count(void);
void mock_gl_set_query_result(GLuint id, GLuint64 result);

/* Debug specific mocks */
void mock_gl_trigger_debug_callback(GLenum source, GLenum type, GLuint id,
                                    GLenum severity, const char* message);

#endif /* MOCK_GL_STANDALONE_H */
