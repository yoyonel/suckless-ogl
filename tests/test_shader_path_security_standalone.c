#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include headers from the project */
#include "glad/glad.h"
#include "log.h"

/* Mock Logging Implementation */
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
	/* In a real test, we might capture this output */
}
void log_set_callback(LogCallback callback)
{
	(void)callback;
}
void log_set_level(LogLevel level)
{
	(void)level;
}
LogLevel log_get_level(void)
{
	return LOG_LEVEL_INFO;
}

/* Mock OpenGL Types and Functions */
/* Since glad.h only declares them, we must define them here */
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
void glDeleteTextures(GLsizei n, const GLuint* textures)
{
	(void)n;
	(void)textures;
}

/* Include implementation to test internal static functions */
#include "shader.c"

/* Test Helpers */
#define ASSERT_TRUE(cond, msg)                      \
	if (!(cond)) {                              \
		fprintf(stderr, "FAIL: %s\n", msg); \
		exit(1);                            \
	} else {                                    \
		printf("PASS: %s\n", msg);          \
	}

#define ASSERT_FALSE(cond, msg)                     \
	if (cond) {                                 \
		fprintf(stderr, "FAIL: %s\n", msg); \
		exit(1);                            \
	} else {                                    \
		printf("PASS: %s\n", msg);          \
	}

/* Test Cases */

void test_get_dir_from_path_truncation(void)
{
	char out_buf[10];
	const char* long_path = "/home/user/very/long/path/to/file.glsl";
	// Directory is "/home/user/very/long/path/to/" -> len 29
	// Buffer size is 10
	// Expected: Failure (return false)

	bool result = get_dir_from_path(long_path, out_buf, sizeof(out_buf));
	ASSERT_FALSE(result, "get_dir_from_path should fail on truncation");
}

void test_get_dir_from_path_success(void)
{
	char out_buf[30];
	const char* path = "/tmp/file.glsl";
	// Directory is "/tmp/" -> len 5
	// Buffer size is 30
	// Expected: Success (return true), buffer contains "/tmp/"

	bool result = get_dir_from_path(path, out_buf, sizeof(out_buf));
	ASSERT_TRUE(result, "get_dir_from_path should succeed");
	ASSERT_TRUE(strcmp(out_buf, "/tmp/") == 0,
	            "get_dir_from_path output correct");
}

void test_parse_include_path_truncation(void)
{
	char out_buf[5];
	const char* input = "\"header.glsl\"";  // Length 11 ("header.glsl")
	// Buffer size 5
	// Expected: Failure (return NULL)

	const char* result =
	    parse_include_path(input, out_buf, sizeof(out_buf));
	ASSERT_TRUE(result == NULL,
	            "parse_include_path should fail on truncation");
}

void test_parse_include_path_success(void)
{
	char out_buf[20];
	const char* input = "\"header.glsl\"";  // Length 11
	// Buffer size 20
	// Expected: Success (return non-NULL), buffer contains "header.glsl"

	const char* result =
	    parse_include_path(input, out_buf, sizeof(out_buf));
	ASSERT_TRUE(result != NULL, "parse_include_path should succeed");
	ASSERT_TRUE(strcmp(out_buf, "header.glsl") == 0,
	            "parse_include_path output correct");
}

int main(void)
{
	printf("Running Shader Path Security Tests...\n");

	test_get_dir_from_path_truncation();
	test_get_dir_from_path_success();
	test_parse_include_path_truncation();
	test_parse_include_path_success();

	printf("All tests passed!\n");
	return 0;
}
