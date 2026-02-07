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

void test_texture_upload_excessive_dimensions(void)
{
	// The implementation enforces a limit of 8192.
	// We test 8193 to ensure it is rejected.
	int width = 8193;
	int height = 16;
	float dummy_data[64] = {0};

	GLuint tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_EQUAL_MESSAGE(
	    0, tex,
	    "Should reject texture with width > MAX_TEXTURE_DIMENSION (8192)");

	width = 16;
	height = 8193;
	tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_EQUAL_MESSAGE(
	    0, tex,
	    "Should reject texture with height > MAX_TEXTURE_DIMENSION (8192)");
}

void test_texture_upload_valid_dimensions(void)
{
	int width = 4;
	int height = 4;
	// 4x4 * 4 floats * sizeof(float) = 64 bytes
	float dummy_data[64] = {0};

	GLuint tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_NOT_EQUAL(0, tex);

	glDeleteTextures(1, &tex);
}

void test_texture_load_excessive_dimensions_dos(void)
{
	const char* filename = "test_dos_9000x1.ppm";
	FILE* f = fopen(filename, "wb");
	TEST_ASSERT_NOT_NULL_MESSAGE(f, "Failed to create test file");
	fprintf(f, "P6\n9000 1\n255\n");
	fclose(f);

	/* Should return 0 (NULL) because dimensions exceed limit */
	GLuint tex = texture_load(filename);
	TEST_ASSERT_EQUAL_MESSAGE(0, tex,
	                          "Should reject texture with width > "
	                          "MAX_TEXTURE_DIMENSION (8192) during load");

	remove(filename);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_upload_excessive_dimensions);
	RUN_TEST(test_texture_upload_valid_dimensions);
	RUN_TEST(test_texture_load_excessive_dimensions_dos);
	return UNITY_END();
}
