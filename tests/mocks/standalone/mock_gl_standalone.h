#ifndef MOCK_GL_STANDALONE_H
#define MOCK_GL_STANDALONE_H

#include <glad/glad.h>

#include <stdint.h>

/* Defaults */
#define DEFAULT_BUFFER_ID 123
#define DEFAULT_VAO_ID 456

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
