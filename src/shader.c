#include "shader.h"

#include <stdio.h>
#include <stdlib.h>

#include "glad/glad.h"
#include "log.h"

enum { INFO_LOG_SIZE = 512 };

static char* read_file(const char* path)
{
	FILE* file_ptr = fopen(path, "rb");
	if (!file_ptr) {
		perror("fopen");
		return NULL;
	}

	if (fseek(file_ptr, 0, SEEK_END) != 0) {
		(void)fclose(file_ptr);
		return NULL;
	}
	long len = ftell(file_ptr);
	if (len < 0) {
		(void)fclose(file_ptr);
		return NULL;
	}
	size_t size = (size_t)len;
	char* src = malloc(size + 1);
	if (!src) {
		(void)fclose(file_ptr);
		return NULL;
	}

	if (fseek(file_ptr, 0, SEEK_SET) != 0) {
		(void)fclose(file_ptr);
		free(src);
		return NULL;
	}

	size_t read_bytes = fread(src, 1, size, file_ptr);
	(void)fclose(file_ptr);

	if (read_bytes > size) {
		read_bytes = size;
	}
	src[read_bytes] = '\0';  // NOLINT

	return src;
}

GLuint shader_compile(const char* path, GLenum type)
{
	char* src = read_file(path);
	if (!src) {
		LOG_ERROR("suckless-ogl.shader",
		          "Failed to read shader file: %s", path);
		return 0;
	}

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, (const char**)&src, NULL);
	glCompileShader(shader);
	free(src);

	int success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == 0) {
		char log[INFO_LOG_SIZE];
		glGetShaderInfoLog(shader, INFO_LOG_SIZE, NULL, log);
		LOG_ERROR("suckless-ogl.shader",
		          "Shader compilation error (%s):\n%s", path, log);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint shader_load_program(const char* vertex_path, const char* fragment_path)
{
	GLuint vertex_shader = shader_compile(vertex_path, GL_VERTEX_SHADER);
	if (vertex_shader == 0) {
		return 0;
	}

	GLuint fragment_shader =
	    shader_compile(fragment_path, GL_FRAGMENT_SHADER);
	if (fragment_shader == 0) {
		glDeleteShader(vertex_shader);
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	int success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == 0) {
		char log[INFO_LOG_SIZE];
		glGetProgramInfoLog(program, INFO_LOG_SIZE, NULL, log);
		LOG_ERROR("suckless-ogl.shader", "Shader linking error:\n%s",
		          log);
		glDeleteProgram(program);
		program = 0;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program;
}

GLuint shader_load_compute(const char* compute_path)
{
	GLuint compute_shader = shader_compile(compute_path, GL_COMPUTE_SHADER);
	if (compute_shader == 0) {
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, compute_shader);
	glLinkProgram(program);

	int success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == 0) {
		char log[INFO_LOG_SIZE];
		glGetProgramInfoLog(program, INFO_LOG_SIZE, NULL, log);
		LOG_ERROR("suckless-ogl.shader",
		          "Compute shader linking error:\n%s", log);
		glDeleteProgram(program);
		program = 0;
	}

	glDeleteShader(compute_shader);

	return program;
}
