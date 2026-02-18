#ifndef MOCK_GL_STANDALONE_H
#define MOCK_GL_STANDALONE_H

#include <stddef.h>
#include "glad/glad.h"

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
void glPopDebugGroup(void);

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
