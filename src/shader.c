#include "shader.h"

#include <stdio.h>
#include <stdlib.h>

static char* read_file(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) {
		perror("fopen");
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	rewind(f);

	char* src = malloc(len + 1);
	if (!src) {
		fclose(f);
		return NULL;
	}

	size_t read = fread(src, 1, len, f);
	src[read] = '\0';
	fclose(f);

	return src;
}

GLuint shader_compile(const char* path, GLenum type)
{
	char* src = read_file(path);
	if (!src) {
		fprintf(stderr, "Failed to read shader file: %s\n", path);
		return 0;
	}

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, (const char**)&src, NULL);
	glCompileShader(shader);
	free(src);

	int success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char log[512];
		glGetShaderInfoLog(shader, 512, NULL, log);
		fprintf(stderr, "Shader compilation error (%s):\n%s\n", path,
		        log);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint shader_load_program(const char* vertex_path, const char* fragment_path)
{
	GLuint vs = shader_compile(vertex_path, GL_VERTEX_SHADER);
	if (!vs) {
		return 0;
	}

	GLuint fs = shader_compile(fragment_path, GL_FRAGMENT_SHADER);
	if (!fs) {
		glDeleteShader(vs);
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	int success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		char log[512];
		glGetProgramInfoLog(program, 512, NULL, log);
		fprintf(stderr, "Shader linking error:\n%s\n", log);
		glDeleteProgram(program);
		program = 0;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}

GLuint shader_load_compute(const char* compute_path)
{
	GLuint cs = shader_compile(compute_path, GL_COMPUTE_SHADER);
	if (!cs) {
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, cs);
	glLinkProgram(program);

	int success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		char log[512];
		glGetProgramInfoLog(program, 512, NULL, log);
		fprintf(stderr, "Compute shader linking error:\n%s\n", log);
		glDeleteProgram(program);
		program = 0;
	}

	glDeleteShader(cs);

	return program;
}
