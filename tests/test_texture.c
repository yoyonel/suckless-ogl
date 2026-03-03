// tests/test_texture.c
#include "simd_utils.h"
#include "texture.h"
#include "unity.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLFWwindow* window = NULL;

static const int WINDOW_WIDTH = 640;
static const int WINDOW_HEIGHT = 480;
static const int GL_VER_MAJOR = 3;
static const int GL_VER_MINOR = 3;
static const int INITIAL_DIMENSION = 0;
static const int TEX_LEVEL_0 = 0;
static const GLuint INVALID_TEX = 0;
static const int DELETE_COUNT = 1;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	// Hidden window for headless testing
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VER_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VER_MINOR);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Test Window",
	                          NULL, NULL);
	if (!window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}
}

void tearDown(void)
{
	if (window) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();
}

void test_texture_load_pixels_invalid_path(void)
{
	int width = INITIAL_DIMENSION;
	int height = INITIAL_DIMENSION;
	int channels = 0;
	float* data = texture_load_pixels("non_existent_file.hdr", &width,
	                                  &height, &channels);

	// Should return NULL for invalid file
	TEST_ASSERT_NULL(data);
}

void test_texture_integration_success(void)
{
	int width = INITIAL_DIMENSION;
	int height = INITIAL_DIMENSION;
	int channels = 0;

	// Use the actual HDR file from assets (relative to build/tests)
	float* data =
	    texture_load_pixels("assets/textures/hdr/abandoned_garage_4k.hdr",
	                        &width, &height, &channels);

	TEST_ASSERT_NOT_NULL(data);
	// Should have valid dimensions
	TEST_ASSERT_GREATER_THAN(INITIAL_DIMENSION, width);
	TEST_ASSERT_GREATER_THAN(INITIAL_DIMENSION, height);

	// Convert to half-float
	size_t pixel_count = (size_t)width * (size_t)height * 4;
	uint16_t* half_data = malloc(pixel_count * sizeof(uint16_t));
	TEST_ASSERT_NOT_NULL(half_data);
	convert_float_to_half_simd(data, half_data, pixel_count);

	// Create PBO, allocate, map, and copy data (matching async_loader flow)
	GLuint pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	GLsizeiptr pbo_size = (GLsizeiptr)(pixel_count * sizeof(uint16_t));
	glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_size, NULL, GL_STREAM_DRAW);
	void* mapped = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	TEST_ASSERT_NOT_NULL(mapped);
	memcpy(mapped, half_data, (size_t)pbo_size);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	GLuint tex = texture_upload_hdr_from_pbo(pbo, mapped, width, height, 0);
	glDeleteBuffers(1, &pbo);
	free(data);
	free(half_data);

	// Should return non-zero texture ID
	TEST_ASSERT_NOT_EQUAL(INVALID_TEX, tex);

	// Verify it's a valid OpenGL texture
	TEST_ASSERT_TRUE(glIsTexture(tex));

	glDeleteTextures(DELETE_COUNT, &tex);
}

void test_texture_upload_hdr_properties(void)
{
	int width = INITIAL_DIMENSION;
	int height = INITIAL_DIMENSION;
	int channels = 0;
	float* data =
	    texture_load_pixels("assets/textures/hdr/abandoned_garage_4k.hdr",
	                        &width, &height, &channels);
	TEST_ASSERT_NOT_NULL(data);

	// Convert to half-float
	size_t pixel_count = (size_t)width * (size_t)height * 4;
	uint16_t* half_data = malloc(pixel_count * sizeof(uint16_t));
	TEST_ASSERT_NOT_NULL(half_data);
	convert_float_to_half_simd(data, half_data, pixel_count);

	// Create PBO, allocate, map, and copy data (matching async_loader flow)
	GLuint pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
	GLsizeiptr pbo_size = (GLsizeiptr)(pixel_count * sizeof(uint16_t));
	glBufferData(GL_PIXEL_UNPACK_BUFFER, pbo_size, NULL, GL_STREAM_DRAW);
	void* mapped = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	TEST_ASSERT_NOT_NULL(mapped);
	memcpy(mapped, half_data, (size_t)pbo_size);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	GLuint tex = texture_upload_hdr_from_pbo(pbo, mapped, width, height, 0);
	glDeleteBuffers(1, &pbo);
	free(data);
	free(half_data);
	TEST_ASSERT_NOT_EQUAL(INVALID_TEX, tex);

	// Bind and verify texture properties
	glBindTexture(GL_TEXTURE_2D, tex);

	GLint internal_format = INITIAL_DIMENSION;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, TEX_LEVEL_0,
	                         GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

	// Should be RGBA16F format
	TEST_ASSERT_EQUAL(GL_RGBA16F, internal_format);

	// Verify dimensions match what was returned
	GLint tex_width = INITIAL_DIMENSION;
	GLint tex_height = INITIAL_DIMENSION;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, TEX_LEVEL_0, GL_TEXTURE_WIDTH,
	                         &tex_width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, TEX_LEVEL_0, GL_TEXTURE_HEIGHT,
	                         &tex_height);

	TEST_ASSERT_EQUAL(width, tex_width);
	TEST_ASSERT_EQUAL(height, tex_height);

	// Verify texture parameters
	GLint min_filter = INITIAL_DIMENSION;
	GLint mag_filter = INITIAL_DIMENSION;
	GLint wrap_s = INITIAL_DIMENSION;
	GLint wrap_t = INITIAL_DIMENSION;

	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_filter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag_filter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap_s);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrap_t);

	TEST_ASSERT_EQUAL(GL_LINEAR_MIPMAP_LINEAR, min_filter);
	TEST_ASSERT_EQUAL(GL_LINEAR, mag_filter);
	TEST_ASSERT_EQUAL(GL_REPEAT, wrap_s);
	TEST_ASSERT_EQUAL(GL_CLAMP_TO_EDGE, wrap_t);

	glBindTexture(GL_TEXTURE_2D, INVALID_TEX);
	glDeleteTextures(DELETE_COUNT, &tex);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_load_pixels_invalid_path);
	RUN_TEST(test_texture_integration_success);
	RUN_TEST(test_texture_upload_hdr_properties);
	return UNITY_END();
}
