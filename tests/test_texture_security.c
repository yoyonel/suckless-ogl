#include "texture.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

static GLFWwindow* window = NULL;

enum {
	WINDOW_WIDTH = 640,
	WINDOW_HEIGHT = 480,
	TEST_DIM = 16,
	DUMMY_DATA_SIZE = 64
};

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
	// The implementation enforces a limit of MAX_TEXTURE_DIMENSION.
	// We test MAX_TEXTURE_DIMENSION + 1 to ensure it is rejected.
	int width = MAX_TEXTURE_DIMENSION + 1;
	int height = TEST_DIM;
	float dummy_data[DUMMY_DATA_SIZE] = {0};

	GLuint tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_EQUAL_MESSAGE(
	    0, tex, "Should reject texture with width > MAX_TEXTURE_DIMENSION");

	width = TEST_DIM;
	height = MAX_TEXTURE_DIMENSION + 1;
	tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_EQUAL_MESSAGE(
	    0, tex,
	    "Should reject texture with height > MAX_TEXTURE_DIMENSION");
}

void test_texture_upload_valid_dimensions(void)
{
	int width = 4;
	int height = 4;
	// 4x4 * 4 floats * sizeof(float) = 64 bytes
	float dummy_data[DUMMY_DATA_SIZE] = {0};

	GLuint tex = texture_upload_hdr(dummy_data, width, height);
	TEST_ASSERT_NOT_EQUAL(0, tex);

	glDeleteTextures(1, &tex);
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
	// check. If it tries to allocate first, it might succeed (if memory
	// available) or crash/DoS. But we expect 0 returned due to check.
	GLuint tex = texture_load(bomb_path);
	TEST_ASSERT_EQUAL(0, tex);

	if (remove(bomb_path) != 0) {
		TEST_FAIL_MESSAGE("Failed to remove temporary bomb file");
	}
}

void test_texture_load_non_existent_file(void)
{
	GLuint tex = texture_load("non_existent_file.png");
	TEST_ASSERT_EQUAL(0, tex);
}

void test_texture_load_invalid_file(void)
{
	const char* invalid_path = "invalid.png";
	FILE* file = fopen(invalid_path, "wb");
	TEST_ASSERT_NOT_NULL(file);
	fprintf(file, "NOT A REAL IMAGE");
	fclose(file);

	GLuint tex = texture_load(invalid_path);
	TEST_ASSERT_EQUAL(0, tex);

	remove(invalid_path);
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
	fclose(file);

	int width = 0;
	int height = 0;
	int channels = 0;
	float* data =
	    texture_load_pixels(invalid_path, &width, &height, &channels);
	TEST_ASSERT_NULL(data);

	remove(invalid_path);
}

void test_texture_upload_null_data(void)
{
	GLuint tex = texture_upload_hdr(NULL, 16, 16);
	TEST_ASSERT_EQUAL(0, tex);
}

void test_texture_load_truncated_data(void)
{
	const char* truncated_path = "truncated.ppm";
	FILE* file = fopen(truncated_path, "wb");
	TEST_ASSERT_NOT_NULL(file);
	// Valid header, but not enough data
	int printed = fprintf(file, "P6\n2 2\n255\n");
	TEST_ASSERT_GREATER_THAN(0, printed);
	// Should be 2*2*3 = 12 bytes, write only 1
	int put = fputc(0, file);
	TEST_ASSERT_NOT_EQUAL(EOF, put);
	fclose(file);

	GLuint tex = texture_load(truncated_path);
	TEST_ASSERT_EQUAL(0, tex);

	remove(truncated_path);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_upload_excessive_dimensions);
	RUN_TEST(test_texture_upload_valid_dimensions);
	RUN_TEST(test_texture_load_huge_header_dos);
	RUN_TEST(test_texture_load_non_existent_file);
	RUN_TEST(test_texture_load_invalid_file);
	RUN_TEST(test_texture_load_pixels_non_existent_file);
	RUN_TEST(test_texture_load_pixels_invalid_file);
	RUN_TEST(test_texture_upload_null_data);
	RUN_TEST(test_texture_load_truncated_data);
	return UNITY_END();
}
