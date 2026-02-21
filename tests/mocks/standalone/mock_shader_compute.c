#include "shader.h"
#include <stdlib.h>

GLuint shader_load_compute(const char* compute_path)
{
	(void)compute_path;
	return 100;  // Mock program handle
}

GLuint shader_compile(const char* path, GLenum type)
{
	(void)path;
	(void)type;
	return 200;
}

char* shader_read_file(const char* path)
{
	(void)path;
	return NULL;
}
