// tests/test_texture_security.c
#include "texture.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

static GLFWwindow* window = NULL;

enum {
	WINDOW_WIDTH = 640,
	WINDOW_HEIGHT = 480,
	GL_VERSION_MAJOR = 3,
	GL_VERSION_MINOR = 3,
	MAX_DIMENSION = 8192,
	EXCESSIVE_DIMENSION = 8193,
	SMALL_DIMENSION = 16,
	TEST_DIMENSION_4 = 4,
	DUMMY_DATA_SIZE = 64
};
static const GLuint INVALID_TEXTURE = 0;

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	// Hidden window for headless testing
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VERSION_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VERSION_MINOR);
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

void test_texture_upload_excessive_dimensions(void)
{
	// The implementation enforces a limit of 8192.
	// We test 8193 to ensure it is rejected.
	int width = EXCESSIVE_DIMENSION;
	int height = SMALL_DIMENSION;
	float dummy_data[DUMMY_DATA_SIZE] = {0};

	GLuint tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_EQUAL_MESSAGE(
	    INVALID_TEXTURE, tex,
	    "Should reject texture with width > MAX_TEXTURE_DIMENSION (8192)");

	width = SMALL_DIMENSION;
	height = EXCESSIVE_DIMENSION;
	tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_EQUAL_MESSAGE(
	    INVALID_TEXTURE, tex,
	    "Should reject texture with height > MAX_TEXTURE_DIMENSION (8192)");
}

void test_texture_upload_valid_dimensions(void)
{
	int width = TEST_DIMENSION_4;
	int height = TEST_DIMENSION_4;
	// 4x4 * 4 floats * sizeof(float) = 64 bytes
	float dummy_data[DUMMY_DATA_SIZE] = {0};

	GLuint tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_NOT_EQUAL(INVALID_TEXTURE, tex);

	glDeleteTextures(1, &tex);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_upload_excessive_dimensions);
	RUN_TEST(test_texture_upload_valid_dimensions);
	return UNITY_END();
}
