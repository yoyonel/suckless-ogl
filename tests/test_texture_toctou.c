#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock Dependencies
#include "glad/glad.h"
#include "log.h"
#include "stb_image.h"

// Define mocks for log functions
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
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

// Mock OpenGL functions
void glGenTextures(GLsizei n, GLuint* textures)
{
	(void)n;
	if (textures) *textures = 1;
}
void glBindTexture(GLenum target, GLuint texture)
{
	(void)target;
	(void)texture;
}
void glPixelStorei(GLenum pname, GLint param)
{
	(void)pname;
	(void)param;
}
void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                    GLsizei width, GLsizei height)
{
	(void)target;
	(void)levels;
	(void)internalformat;
	(void)width;
	(void)height;
}
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const void* pixels)
{
	(void)target;
	(void)level;
	(void)xoffset;
	(void)yoffset;
	(void)width;
	(void)height;
	(void)format;
	(void)type;
	(void)pixels;
}
void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	(void)target;
	(void)pname;
	(void)param;
}
void glGenerateMipmap(GLenum target)
{
	(void)target;
}
GLenum glGetError(void)
{
	return 0;
}
void glDeleteTextures(GLsizei n, const GLuint* textures)
{
	(void)n;
	(void)textures;
}
void glGetIntegerv(GLenum pname, GLint* data)
{
	(void)pname;
	(void)data;
}

// STB Image Mocks
int stbi_info_from_file(FILE* f, int* x, int* y, int* comp)
{
	(void)f;
	// Return valid dimensions
	*x = 10;
	*y = 10;
	*comp = 4;
	return 1;  // Success
}

float* stbi_loadf_from_file(FILE* f, int* x, int* y, int* channels_in_file,
                            int desired_channels)
{
	(void)f;
	(void)channels_in_file;
	(void)desired_channels;

	// Return HUGE dimensions (TOCTOU simulation)
	// MAX_TEXTURE_DIMENSION is 8192
	*x = 9000;
	*y = 9000;

	// Allocate dummy data
	float* data = (float*)malloc(sizeof(float) * 4);
	return data;
}

void stbi_image_free(void* retval_from_stbi_load)
{
	free(retval_from_stbi_load);
}

// Include source under test
#include "../src/texture.c"

int main(void)
{
	printf("Running Texture TOCTOU Security Test...\n");

	// Create a dummy file
	const char* filename = "dummy_toctou.hdr";
	FILE* f = fopen(filename, "w");
	if (!f) {
		fprintf(stderr, "Failed to create dummy file\n");
		return 1;
	}
	fprintf(f, "DUMMY");
	fclose(f);

	int w, h, c;
	// This should fail (return NULL) if security check is present
	// Currently (before fix), it will return a pointer because
	// stbi_loadf_from_file returns a pointer and texture_load_pixels only
	// checks stbi_info dimensions (which are 10x10).
	float* data = texture_load_pixels(filename, &w, &h, &c);

	remove(filename);

	if (data != NULL) {
		printf(
		    "FAIL: texture_load_pixels returned data despite dimension "
		    "mismatch/overflow!\n");
		printf("Returned dimensions: %dx%d\n", w, h);
		stbi_image_free(data);
		return 1;
	} else {
		printf(
		    "PASS: texture_load_pixels returned NULL (rejected invalid "
		    "dimensions)\n");
		return 0;
	}
}
