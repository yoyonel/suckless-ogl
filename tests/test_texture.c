#include "texture.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

static GLFWwindow* window = NULL;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	// Hidden window for headless testing
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(640, 480, "Test Window", NULL, NULL);
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
	int width = 0, height = 0;
	GLuint tex = texture_load_hdr("non_existent_file.hdr", &width, &height);

	// Should return 0 for invalid file
	TEST_ASSERT_EQUAL(0, tex);
}

void test_texture_load_hdr_success(void)
{
	int width = 0, height = 0;

	// Use the actual HDR file from assets (relative to build/tests)
	GLuint tex = texture_load_hdr("assets/env.hdr", &width, &height);

	// Should return non-zero texture ID
	TEST_ASSERT_NOT_EQUAL(0, tex);

	// Should have valid dimensions
	TEST_ASSERT_GREATER_THAN(0, width);
	TEST_ASSERT_GREATER_THAN(0, height);

	// Verify it's a valid OpenGL texture
	TEST_ASSERT_TRUE(glIsTexture(tex));

	glDeleteTextures(1, &tex);
}

void test_texture_load_hdr_creates_gl_texture(void)
{
	int width = 0, height = 0;
	GLuint tex = texture_load_hdr("assets/env.hdr", &width, &height);

	TEST_ASSERT_NOT_EQUAL(0, tex);

	// Bind and verify texture properties
	glBindTexture(GL_TEXTURE_2D, tex);

	GLint internal_format = 0;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
	                         &internal_format);

	// Should be RGB16F format
	TEST_ASSERT_EQUAL(GL_RGB16F, internal_format);

	// Verify dimensions match what was returned
	GLint tex_width = 0, tex_height = 0;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
	                         &tex_width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
	                         &tex_height);

	TEST_ASSERT_EQUAL(width, tex_width);
	TEST_ASSERT_EQUAL(height, tex_height);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &tex);
}

void test_texture_load_hdr_sets_parameters(void)
{
	int width = 0, height = 0;
	GLuint tex = texture_load_hdr("assets/env.hdr", &width, &height);

	TEST_ASSERT_NOT_EQUAL(0, tex);

	glBindTexture(GL_TEXTURE_2D, tex);

	// Verify texture parameters
	GLint min_filter = 0, mag_filter = 0;
	GLint wrap_s = 0, wrap_t = 0;

	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_filter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag_filter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap_s);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrap_t);

	TEST_ASSERT_EQUAL(GL_LINEAR_MIPMAP_LINEAR, min_filter);
	TEST_ASSERT_EQUAL(GL_LINEAR, mag_filter);
	TEST_ASSERT_EQUAL(GL_REPEAT, wrap_s);
	TEST_ASSERT_EQUAL(GL_CLAMP_TO_EDGE, wrap_t);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &tex);
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
