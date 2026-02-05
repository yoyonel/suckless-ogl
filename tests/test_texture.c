// tests/test_texture.c
#include "texture.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

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

void test_texture_load_hdr_invalid_path(void)
{
	int width = INITIAL_DIMENSION;
	int height = INITIAL_DIMENSION;
	GLuint tex = texture_load_hdr("non_existent_file.hdr", &width, &height);

	// Should return 0 for invalid file
	TEST_ASSERT_EQUAL(INVALID_TEX, tex);
}

void test_texture_load_hdr_success(void)
{
	int width = INITIAL_DIMENSION;
	int height = INITIAL_DIMENSION;

	// Use the actual HDR file from assets (relative to build/tests)
	GLuint tex = texture_load_hdr(
	    "assets/textures/hdr/abandoned_garage_4k.hdr", &width, &height);

	// Should return non-zero texture ID
	TEST_ASSERT_NOT_EQUAL(INVALID_TEX, tex);

	// Should have valid dimensions
	TEST_ASSERT_GREATER_THAN(INITIAL_DIMENSION, width);
	TEST_ASSERT_GREATER_THAN(INITIAL_DIMENSION, height);

	// Verify it's a valid OpenGL texture
	TEST_ASSERT_TRUE(glIsTexture(tex));

	glDeleteTextures(DELETE_COUNT, &tex);
}

void test_texture_load_hdr_creates_gl_texture(void)
{
	int width = INITIAL_DIMENSION;
	int height = INITIAL_DIMENSION;
	GLuint tex = texture_load_hdr(
	    "assets/textures/hdr/abandoned_garage_4k.hdr", &width, &height);

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

	glBindTexture(GL_TEXTURE_2D, INVALID_TEX);
	glDeleteTextures(DELETE_COUNT, &tex);
}

void test_texture_load_hdr_sets_parameters(void)
{
	int width = INITIAL_DIMENSION;
	int height = INITIAL_DIMENSION;
	GLuint tex = texture_load_hdr(
	    "assets/textures/hdr/abandoned_garage_4k.hdr", &width, &height);

	TEST_ASSERT_NOT_EQUAL(INVALID_TEX, tex);

	glBindTexture(GL_TEXTURE_2D, tex);

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
	RUN_TEST(test_texture_load_hdr_invalid_path);
	RUN_TEST(test_texture_load_hdr_success);
	RUN_TEST(test_texture_load_hdr_creates_gl_texture);
	RUN_TEST(test_texture_load_hdr_sets_parameters);
	return UNITY_END();
}
