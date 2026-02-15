#include "texture.h"
#include "unity.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	uint16_t dummy_data[DUMMY_DATA_SIZE] = {0};

	// We can pass 0 for pbo_id because the dimension check happens
	// before any PBO operations.
	GLuint tex = texture_upload_hdr_from_pbo(0, width, height, 0);
	TEST_ASSERT_EQUAL_MESSAGE(
	    0, tex, "Should reject texture with width > MAX_TEXTURE_DIMENSION");

	width = TEST_DIM;
	height = MAX_TEXTURE_DIMENSION + 1;
	tex = texture_upload_hdr_from_pbo(0, width, height, 0);
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

	GLuint tex = texture_upload_hdr_from_pbo(pbo, width, height, 0);
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
	// check.
	int w, h, c;
	float* data = texture_load_pixels(bomb_path, &w, &h, &c);
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
	fclose(file);

	int width = 0;
	int height = 0;
	int channels = 0;
	float* data =
	    texture_load_pixels(invalid_path, &width, &height, &channels);
	TEST_ASSERT_NULL(data);

	remove(invalid_path);
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
