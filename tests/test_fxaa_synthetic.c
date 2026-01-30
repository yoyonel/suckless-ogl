#include <glad/glad.h>

#include "postprocess.h"
#include "shader.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH 1024
#define HEIGHT 768
#define GL_VERSION_MAJOR 4
#define GL_VERSION_MINOR 5

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static GLFWwindow* global_window = NULL;
static PostProcess post_process_system;
static Shader* pattern_shader_ptr = NULL;

/**
 * Flips the image vertically (OpenGL reads bottom-to-top, PNG needs
 * top-to-bottom).
 */
static void flip_image_vertically(int image_width, int image_height,
                                  unsigned char* image_data)
{
	int row_size = image_width * 3;
	unsigned char* row_tmp = malloc((size_t)row_size);
	if (!row_tmp) {
		return;
	}

	for (int y_coord = 0; y_coord < image_height / 2; y_coord++) {
		unsigned char* top_row = image_data + y_coord * row_size;
		unsigned char* bottom_row =
		    image_data + (image_height - y_coord - 1) * row_size;
		memcpy(row_tmp, top_row, (size_t)row_size);
		memcpy(top_row, bottom_row, (size_t)row_size);
		memcpy(bottom_row, row_tmp, (size_t)row_size);
	}
	free(row_tmp);
}

void setUp(void)
{
	if (!glfwInit()) {
		TEST_FAIL_MESSAGE("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_VERSION_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_VERSION_MINOR);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	global_window =
	    glfwCreateWindow(WIDTH, HEIGHT, "FXAA Synthetic Test", NULL, NULL);
	if (!global_window) {
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(global_window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(global_window);
		glfwTerminate();
		TEST_FAIL_MESSAGE("Failed to initialize GLAD");
	}

	if (!postprocess_init(&post_process_system, WIDTH, HEIGHT)) {
		TEST_FAIL_MESSAGE("Failed to initialize PostProcess");
	}

	pattern_shader_ptr = shader_load("shaders/debug_tex.vert",
	                                 "shaders/test/test_pattern.frag");
	if (!pattern_shader_ptr) {
		TEST_FAIL_MESSAGE("Failed to load test pattern shader");
	}
}

void tearDown(void)
{
	if (pattern_shader_ptr) {
		shader_destroy(pattern_shader_ptr);
	}
	postprocess_cleanup(&post_process_system);
	if (global_window) {
		glfwDestroyWindow(global_window);
	}
	glfwTerminate();
}

/**
 * Calculates the average absolute gradient in the image.
 * Aliasing increases local gradients (high-frequency noise).
 * FXAA should reduce this value by smoothing the edges.
 */
static float calculate_edge_noise(int image_width, int image_height,
                                  const unsigned char* image_pixels)
{
	float total_grad = 0.0f;
	int sample_count = 0;

	for (int y_coord = 1; y_coord < image_height - 1; y_coord++) {
		for (int x_coord = 1; x_coord < image_width - 1; x_coord++) {
			int pixel_idx = (y_coord * image_width + x_coord) * 3;
			int pixel_idx_right =
			    (y_coord * image_width + (x_coord + 1)) * 3;
			int pixel_idx_down =
			    ((y_coord + 1) * image_width + x_coord) * 3;

			// Simple gradient: |v(x+1) - v(x)| + |v(y+1) - v(y)|
			float grad_x =
			    (float)abs((int)image_pixels[pixel_idx_right] -
			               (int)image_pixels[pixel_idx]);
			float grad_y =
			    (float)abs((int)image_pixels[pixel_idx_down] -
			               (int)image_pixels[pixel_idx]);

			total_grad += grad_x + grad_y;
			sample_count++;
		}
	}

	return total_grad / (float)sample_count;
}

void test_fxaa_synthetic_efficiency(void)
{
	// 1. Render Pattern to Scene FBO
	postprocess_begin(&post_process_system);
	glViewport(0, 0, WIDTH, HEIGHT);
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	shader_use(pattern_shader_ptr);
	// Bind empty VAO for vertexID-based quad
	GLuint empty_vao = 0;
	glGenVertexArrays(1, &empty_vao);
	glBindVertexArray(empty_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &empty_vao);

	// 2. Capture "Before" (No AA)
	unsigned char* pixels_before = malloc(WIDTH * HEIGHT * 3);
	if (!pixels_before) {
		TEST_FAIL_MESSAGE("Failed to allocate pixels_before");
	}
	glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE,
	             pixels_before);
	float noise_before = calculate_edge_noise(WIDTH, HEIGHT, pixels_before);
	printf("Edge Noise WITHOUT FXAA: %.4f\n", (double)noise_before);

	// 3. Apply FXAA
	postprocess_enable(&post_process_system, POSTFX_FXAA);
	// We need to simulate the post-processing pass
	postprocess_end(&post_process_system);

	// 4. Capture "After" (FXAA enabled)
	unsigned char* pixels_after = malloc(WIDTH * HEIGHT * 3);
	if (!pixels_after) {
		free(pixels_before);
		TEST_FAIL_MESSAGE("Failed to allocate pixels_after");
	}
	glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE,
	             pixels_after);
	float noise_after = calculate_edge_noise(WIDTH, HEIGHT, pixels_after);
	printf("Edge Noise WITH FXAA: %.4f\n", (double)noise_after);

	// 5. Statistics & Validation
	float noise_reduction = (noise_before - noise_after) / noise_before;
	printf("FXAA Noise Reduction: %.2f%%\n",
	       (double)(noise_reduction * 100.0F));

	// 6. Visual Export (PNG)
	flip_image_vertically(WIDTH, HEIGHT, pixels_before);
	flip_image_vertically(WIDTH, HEIGHT, pixels_after);

	if (stbi_write_png("tests/fxaa_test_before.png", WIDTH, HEIGHT, 3,
	                   pixels_before, WIDTH * 3)) {
		printf("[VISUAL] Saved tests/fxaa_test_before.png\n");
	}
	if (stbi_write_png("tests/fxaa_test_after.png", WIDTH, HEIGHT, 3,
	                   pixels_after, WIDTH * 3)) {
		printf("[VISUAL] Saved tests/fxaa_test_after.png\n");
	}

	const float MIN_REDUCTION_THRESHOLD = 0.10F;
	TEST_ASSERT_TRUE_MESSAGE(
	    noise_reduction > MIN_REDUCTION_THRESHOLD,
	    "FXAA should reduce edge noise by at least 10%%");

	free(pixels_before);
	free(pixels_after);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_fxaa_synthetic_efficiency);
	return UNITY_END();
}
