#include <glad/glad.h>

#include "postprocess.h"
#include "render_utils.h"
#include "shader.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 1024
#define HEIGHT 768
#define GL_VERSION_MAJOR 4
#define GL_VERSION_MINOR 5

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define PATH_MAX_LEN 256

// Global state (required for GLFW callbacks and simple test structure)
static GLFWwindow* global_window = NULL;
static PostProcess post_process_system;
static Shader* pattern_shader_ptr = NULL;
static unsigned char* pixels_before = NULL;
static unsigned char* pixels_after = NULL;

static const float LUMA_R = 0.299F;
static const float LUMA_G = 0.587F;
static const float LUMA_B = 0.114F;
static const float MIN_REDUCE_THRESHOLD = 0.10F;
static const float UINT8_MAX_F = 255.0F;

/**
 * Flips the image vertically (OpenGL reads bottom-to-top, PNG needs
 * top-to-bottom).
 */
static void flip_image_vertically(int image_width, int image_height,
                                  unsigned char* image_data)
{
	int row_sz = image_width * 3;
	unsigned char* row_tmp = malloc((size_t)row_sz);
	if (!row_tmp) {
		return;
	}

	for (int y_coord = 0; y_coord < (image_height / 2); y_coord++) {
		unsigned char* top_row = image_data + (y_coord * row_sz);
		unsigned char* bottom_row =
		    image_data + (((image_height - y_coord) - 1) * row_sz);
		memcpy(row_tmp, top_row, (size_t)row_sz);
		memcpy(top_row, bottom_row, (size_t)row_sz);
		memcpy(bottom_row, row_tmp, (size_t)row_sz);
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

	free(pixels_before);
	pixels_before = NULL;
	free(pixels_after);
	pixels_after = NULL;
}

/**
 * Calculates the perceptual luminance of an RGB pixel.
 */
static float calculate_pixel_luma(const unsigned char* pixel)
{
	float r_lin = (float)pixel[0] / UINT8_MAX_F;
	float g_lin = (float)pixel[1] / UINT8_MAX_F;
	float b_lin = (float)pixel[2] / UINT8_MAX_F;

	// Perceptual luma (linear to gamma approx) to match FXAA internal logic
	return sqrtf((r_lin * LUMA_R) + (g_lin * LUMA_G) + (b_lin * LUMA_B));
}

/**
 * Calculates the average absolute gradient in the image.
 * Aliasing increases local gradients (high-frequency noise).
 * FXAA should reduce this value by smoothing the edges.
 * Now using perceptual luminance to handle colored patterns correctly.
 */
static float calculate_edge_noise(int image_width, int image_height,
                                  const unsigned char* image_pixels)
{
	float total_grad_sq = 0.0F;
	int sample_count = 0;

	for (int y_coord = 1; y_coord < (image_height - 1); y_coord++) {
		for (int x_coord = 1; x_coord < (image_width - 1); x_coord++) {
			int idx = (((y_coord * image_width) + x_coord) * 3);
			int idx_r =
			    (((y_coord * image_width) + (x_coord + 1)) * 3);
			int idx_d =
			    ((((y_coord + 1) * image_width) + x_coord) * 3);

			float l_m = calculate_pixel_luma(&image_pixels[idx]);
			float l_r = calculate_pixel_luma(&image_pixels[idx_r]);
			float l_d = calculate_pixel_luma(&image_pixels[idx_d]);

			float diff_x = l_r - l_m;
			float diff_y = l_d - l_m;

			// Squaring gradients penalizes high-frequency jumps
			// (aliasing) more than smoothed transitions.
			total_grad_sq += (diff_x * diff_x) + (diff_y * diff_y);
			sample_count++;
		}
	}

	if (sample_count == 0) {
		return 0.0F;
	}
	return total_grad_sq / (float)sample_count;
}

/**
 * Helper to render the pattern to the scene buffer.
 */
static void render_test_pattern(int mode)
{
	shader_use(pattern_shader_ptr);
	shader_set_int(pattern_shader_ptr, "u_mode", mode);

	GLuint empty_v = 0;
	glGenVertexArrays(1, &empty_v);
	glBindVertexArray(empty_v);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &empty_v);
}

/**
 * Run the FXAA synthetic test for a specific mode and pattern name.
 */
static void run_synthetic_test(int mode, const char* pattern_name)
{
	printf("\n--- FXAA Synthetic Test: %s (Mode %d) ---\n", pattern_name,
	       mode);

	// 1. Capture "Before" (No AA, but processed through tonemapper/gamma)
	postprocess_disable(&post_process_system, POSTFX_FXAA);
	postprocess_begin(&post_process_system);
	glViewport(0, 0, WIDTH, HEIGHT);
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
	glClear((GLbitfield)GL_COLOR_BUFFER_BIT |
	        (GLbitfield)GL_DEPTH_BUFFER_BIT);
	render_test_pattern(mode);
	postprocess_end(&post_process_system);
	glFinish();  // Ensure rendering is complete before reading

	pixels_before = malloc(WIDTH * HEIGHT * 3);
	if (!pixels_before) {
		TEST_FAIL_MESSAGE("Failed to allocate pixels_before");
		return;
	}
	glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE,
	             pixels_before);
	float noise_before = calculate_edge_noise(WIDTH, HEIGHT, pixels_before);
	printf("  Edge Noise WITHOUT FXAA: %.4f\n", (double)noise_before);

	// 2. Capture "After" (FXAA enabled, processed identically)
	postprocess_enable(&post_process_system, POSTFX_FXAA);
	postprocess_begin(&post_process_system);
	glViewport(0, 0, WIDTH, HEIGHT);
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
	glClear((GLbitfield)GL_COLOR_BUFFER_BIT |
	        (GLbitfield)GL_DEPTH_BUFFER_BIT);
	render_test_pattern(mode);
	postprocess_end(&post_process_system);
	glFinish();  // Ensure rendering is complete before reading

	pixels_after = malloc(WIDTH * HEIGHT * 3);
	if (!pixels_after) {
		TEST_FAIL_MESSAGE("Failed to allocate pixels_after");
		return;
	}
	glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE,
	             pixels_after);
	float noise_after = calculate_edge_noise(WIDTH, HEIGHT, pixels_after);
	printf("  Edge Noise WITH FXAA:    %.4f\n", (double)noise_after);

	// 3. Statistics & Validation
	float noise_reduction = (noise_before - noise_after) / noise_before;
	printf("  FXAA Noise Reduction:    %.2f%%\n",
	       (double)(noise_reduction * 100.0F));

	// 4. Visual Export (PNG)
	flip_image_vertically(WIDTH, HEIGHT, pixels_before);
	flip_image_vertically(WIDTH, HEIGHT, pixels_after);

	char gpu_id[PATH_MAX_LEN];
	render_utils_get_gpu_identifier(gpu_id, sizeof(gpu_id));

	char path_b[PATH_MAX_LEN];
	char path_a[PATH_MAX_LEN];
	if (snprintf(path_b, sizeof(path_b), "tests/fxaa_test_%s_before_%s.png",
	             pattern_name, gpu_id) < 0) {
		TEST_FAIL_MESSAGE("snprintf failed for path_b");
	}
	if (snprintf(path_a, sizeof(path_a), "tests/fxaa_test_%s_after_%s.png",
	             pattern_name, gpu_id) < 0) {
		TEST_FAIL_MESSAGE("snprintf failed for path_a");
	}

	if (stbi_write_png(path_b, WIDTH, HEIGHT, 3, pixels_before,
	                   WIDTH * 3) == 0) {
		printf("  [ERROR] Failed to save %s\n", path_b);
	} else {
		printf("  [VISUAL] Saved %s\n", path_b);
	}

	if (stbi_write_png(path_a, WIDTH, HEIGHT, 3, pixels_after, WIDTH * 3) ==
	    0) {
		printf("  [ERROR] Failed to save %s\n", path_a);
	} else {
		printf("  [VISUAL] Saved %s\n", path_a);
	}

	TEST_ASSERT_TRUE_MESSAGE(
	    noise_reduction > MIN_REDUCE_THRESHOLD,
	    "FXAA should reduce edge noise by at least 10%");

	fflush(stdout);
}

void test_fxaa_star(void)
{
	run_synthetic_test(0, "star");
}

void test_fxaa_grid(void)
{
	run_synthetic_test(1, "grid");
}

void test_fxaa_spheres(void)
{
	run_synthetic_test(2, "spheres");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_fxaa_star);
	RUN_TEST(test_fxaa_grid);
	RUN_TEST(test_fxaa_spheres);
	return (UNITY_END() == 0) ? 0 : 1;
}
