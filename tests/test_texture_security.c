#ifndef GL_COMMON_NO_GLAD
#define GL_COMMON_NO_GLAD
#endif
#ifndef GL_COMMON_NO_GLFW
#define GL_COMMON_NO_GLFW
#endif

#include "gl_common.h"
#include "gpu_profiler.h"
#include "texture.h"
#include "unity.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mock implementation of GL functions */
void glGenBuffers(GLsizei n, GLuint* ids)
{
	for (int i = 0; i < n; i++) {
		ids[i] = 100 + i;
	}
}

void glBindBuffer(GLenum target, GLuint id)
{
	(void)target;
	(void)id;
}

void glBufferData(GLenum target, GLsizeiptr size, const void* data,
                  GLenum usage)
{
	(void)target;
	(void)size;
	(void)data;
	(void)usage;
}

void* glMapBuffer(GLenum target, GLenum access)
{
	(void)target;
	(void)access;
	static char dummy[2048];
	return dummy;
}

void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                       GLbitfield access)
{
	(void)target;
	(void)offset;
	(void)length;
	(void)access;
	static char dummy[2048];
	return dummy;
}

GLboolean glUnmapBuffer(GLenum target)
{
	(void)target;
	return GL_TRUE;
}

GLenum glGetError(void)
{
	return GL_NO_ERROR;
}

void glGenTextures(GLsizei n, GLuint* ids)
{
	for (int i = 0; i < n; i++) {
		ids[i] = 200 + i;
	}
}

void glBindTexture(GLenum target, GLuint id)
{
	(void)target;
	(void)id;
}

void glPixelStorei(GLenum pname, GLint param)
{
	(void)pname;
	(void)param;
}

void glTexImage2D(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b,
                  GLenum f, GLenum ty, const void* p)
{
	(void)t;
	(void)l;
	(void)i;
	(void)w;
	(void)h;
	(void)b;
	(void)f;
	(void)ty;
	(void)p;
}

void glTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h,
                     GLenum f, GLenum ty, const void* p)
{
	(void)t;
	(void)l;
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	(void)f;
	(void)ty;
	(void)p;
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

void glDeleteTextures(GLsizei n, const GLuint* ids)
{
	(void)n;
	(void)ids;
}

void glDeleteBuffers(GLsizei n, const GLuint* ids)
{
	(void)n;
	(void)ids;
}

void glPopDebugGroup(void)
{
}

void glPushDebugGroup(GLenum source, GLuint id, GLsizei length,
                      const char* message)
{
	(void)source;
	(void)id;
	(void)length;
	(void)message;
}

void glUseProgram(GLuint program)
{
	(void)program;
}

void glActiveTexture(GLenum texture)
{
	(void)texture;
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

void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname,
                              GLint* params)
{
	(void)target;
	(void)level;
	(void)pname;
	if (params) {
		*params = 64;
	}
}

/* GPU Profiler Mocks */
void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color)
{
	(void)profiler;
	(void)name;
	(void)color;
}

void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	(void)profiler;
}

/** Dummy window pointer to keep old structure if needed, but we don't use it */
static void* const window = NULL;

enum {
	WINDOW_WIDTH = 640,
	WINDOW_HEIGHT = 480,
	TEST_DIM = 16,
	DUMMY_DATA_SIZE = 64
};

void setUp(void)
{
}

void tearDown(void)
{
}

void test_texture_upload_excessive_dimensions(void)
{
	// The implementation enforces a limit of MAX_TEXTURE_DIMENSION.
	// We test MAX_TEXTURE_DIMENSION + 1 to ensure it is rejected.
	int width = MAX_TEXTURE_DIMENSION + 1;
	int height = TEST_DIM;

	// We can pass 0 for pbo_id because the dimension check happens
	// before any PBO operations.
	GLuint tex = texture_upload_hdr_from_pbo(0, NULL, width, height, 0);
	TEST_ASSERT_EQUAL_MESSAGE(
	    0, tex, "Should reject texture with width > MAX_TEXTURE_DIMENSION");

	width = TEST_DIM;
	height = MAX_TEXTURE_DIMENSION + 1;
	tex = texture_upload_hdr_from_pbo(0, NULL, width, height, 0);
	TEST_ASSERT_EQUAL_MESSAGE(
	    0, tex,
	    "Should reject texture with height > MAX_TEXTURE_DIMENSION");
}

void test_texture_upload_valid_dimensions(void)
{
	int width = 4;
	int height = 4;
	// 4x4 * 4 floats * sizeof(float) = 64 bytes
	uint16_t dummy_data[DUMMY_DATA_SIZE] = {0};

	// Create PBO and upload dummy data
	GLuint pbo = 0;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof(dummy_data), NULL,
	             GL_STREAM_DRAW);
	void* pbo_ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	TEST_ASSERT_NOT_NULL_MESSAGE(pbo_ptr, "Failed to map PBO");
	memcpy(pbo_ptr, dummy_data, sizeof(dummy_data));
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	GLuint tex = texture_upload_hdr_from_pbo(pbo, NULL, width, height, 0);
	TEST_ASSERT_NOT_EQUAL(0, tex);

	glDeleteTextures(1, &tex);
	glDeleteBuffers(1, &pbo);
}

void test_texture_load_huge_header_dos(void)
{
	const char* bomb_path = "bomb.ppm";
	FILE* file = fopen(bomb_path, "wb");
	TEST_ASSERT_NOT_NULL(file);
	// P6 (binary PPM), 20000 width, 20000 height, 255 max val
	// No data follows (or minimal data)
	// This declares a 20000x20000 image which would require ~1.6GB RGBA
	// if allocated before checking dimensions.
	int printed = fprintf(file, "P6\n20000 20000\n255\n");
	TEST_ASSERT_GREATER_THAN(0, printed);
	int closed = fclose(file);
	TEST_ASSERT_EQUAL(0, closed);

	// Try to load it. It should fail fast and return 0 because of dimension
	// check.
	int w_out = 0;
	int h_out = 0;
	int c_out = 0;
	float* data = texture_load_pixels(bomb_path, &w_out, &h_out, &c_out);
	TEST_ASSERT_NULL(data);

	if (remove(bomb_path) != 0) {
		TEST_FAIL_MESSAGE("Failed to remove temporary bomb file");
	}
}

void test_texture_load_pixels_non_existent_file(void)
{
	int width = 0;
	int height = 0;
	int channels = 0;
	float* data =
	    texture_load_pixels("non_existent.hdr", &width, &height, &channels);
	TEST_ASSERT_NULL(data);
}

void test_texture_load_pixels_invalid_file(void)
{
	const char* invalid_path = "invalid.hdr";
	FILE* file = fopen(invalid_path, "wb");
	TEST_ASSERT_NOT_NULL(file);
	fprintf(file, "NOT A REAL HDR");
	if (fclose(file) != 0) {
		TEST_FAIL_MESSAGE("Failed to close invalid file");
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	float* data =
	    texture_load_pixels(invalid_path, &width, &height, &channels);
	TEST_ASSERT_NULL(data);

	if (remove(invalid_path) != 0) {
		TEST_FAIL_MESSAGE("Failed to remove invalid file");
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_upload_excessive_dimensions);
	RUN_TEST(test_texture_upload_valid_dimensions);
	RUN_TEST(test_texture_load_huge_header_dos);
	RUN_TEST(test_texture_load_pixels_non_existent_file);
	RUN_TEST(test_texture_load_pixels_invalid_file);
	return UNITY_END();
}
