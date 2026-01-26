#include "app.h"
#include "billboard_rendering.h"
#include "icosphere.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Shared Test App Instance
static App g_app;
static bool g_app_initialized = false;

// Configuration
#define TEST_WIDTH 512
#define TEST_HEIGHT 512
#define MAX_VARIANCE_THRESHOLD 0.005f  // Strict threshold for fireflies
#define MAX_MSE_THRESHOLD 0.02f  // Allow some difference due to approximations

void setUp(void)
{
	if (!g_app_initialized) {
		// Initialize headless-like window
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		int result =
		    app_init(&g_app, TEST_WIDTH, TEST_HEIGHT, "Stability Test");
		TEST_ASSERT_EQUAL_INT(1, result);

		// Setup reasonable defaults for testing
		g_app.show_info_overlay = 0;
		g_app.show_help = 0;
		g_app.show_envmap = 0;  // Disable skybox to focus on sphere

		// Ensure we generated geometry
		icosphere_generate(&g_app.geometry,
		                   3);  // Subdivision 3 is decent reference
		app_update_gpu_buffers(&g_app);

		g_app_initialized = true;
	}
}

void tearDown(void)
{
	// Cleanup handled at end of main
}

// Helper: Capture Framebuffer to float buffer (normalized 0-1)
// Returns newly allocated buffer that must be free()d
float* capture_frame_buffer(void)
{
	size_t pixel_count = TEST_WIDTH * TEST_HEIGHT;
	float* buffer = malloc(pixel_count * 3 * sizeof(float));
	TEST_ASSERT_NOT_NULL(buffer);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, TEST_WIDTH, TEST_HEIGHT, GL_RGB, GL_FLOAT, buffer);
	return buffer;
}

// Helper: Compute MSE between two images
float compute_mse(float* imgA, float* imgB, int width, int height)
{
	double sum_sq_err = 0.0;
	size_t total_pixels = width * height;

	for (size_t i = 0; i < total_pixels * 3; i++) {
		float diff = imgA[i] - imgB[i];
		sum_sq_err += diff * diff;
	}

	return (float)(sum_sq_err / (double)(total_pixels * 3));
}

// Helper: Save RGB float buffer to PPM file
void save_ppm(const char* filename, float* buffer, int width, int height,
              float scale)
{
	FILE* f = fopen(filename, "wb");
	if (!f)
		return;

	fprintf(f, "P6\n%d %d\n255\n", width, height);

	for (int i = 0; i < width * height * 3; i++) {
		float val = buffer[i] * scale;
		if (val < 0.0f)
			val = 0.0f;
		if (val > 1.0f)
			val = 1.0f;
		unsigned char c = (unsigned char)(val * 255.0f);
		fputc(c, f);
	}
	fclose(f);
	printf("Saved %s\n", filename);
}

// Helper: Compute Average and Max Pixel Variance
void compute_temporal_stats(float* frame1, float* frame2, int width, int height,
                            float* out_avg, float* out_max)
{
	double energy = 0.0;
	float max_diff = 0.0f;
	size_t total_pixels = width * height;

	for (size_t i = 0; i < total_pixels * 3; i++) {
		float diff = fabs(frame1[i] - frame2[i]);
		energy += diff;
		if (diff > max_diff)
			max_diff = diff;
	}

	*out_avg = (float)(energy / (double)(total_pixels * 3));
	*out_max = max_diff;
}

void test_static_fidelity_multi_angle(void)
{
	// Define test positions: Center, Grazing Angle, Far, Close
	vec3 test_positions[] = {
	    {0.0f, 0.0f, 2.5f},  // Center (Base)
	    {1.5f, 0.0f, 2.0f},  // Side/Grazing
	    {0.0f, 1.5f, 2.0f},  // Top/Grazing
	    {0.0f, 0.0f, 5.0f},  // Far
	};
	int num_positions = sizeof(test_positions) / sizeof(vec3);

	float total_mse = 0.0f;
	float max_mse_found = 0.0f;

	for (int i = 0; i < num_positions; i++) {
		// 1. Setup Scene
		glm_vec3_copy(test_positions[i], g_app.camera.position);

		// Point camera at origin (0,0,0)
		// Simple lookAt logic for testing (yaw/pitch approx)
		// For strict control, we just use lookAt matrix in app,
		// but here we rely on the app's camera system which uses
		// yaw/pitch. Let's just manually set yaw/pitch for known
		// positions or use a helper if available. For this test, manual
		// updates for the specific positions:

		if (fabs(test_positions[i][0]) > 0.1f) {
			g_app.camera.yaw = -120.0f;  // Approx for side
		} else if (fabs(test_positions[i][1]) > 0.1f) {
			g_app.camera.pitch = -45.0f;  // Approx for top
		} else {
			g_app.camera.yaw = -90.0f;
			g_app.camera.pitch = 0.0f;
		}
		camera_update_vectors(&g_app.camera);

		// 2. Render REFERENCE (Mesh)
		g_app.billboard_mode = 0;
		app_render(&g_app);
		float* ref_img = capture_frame_buffer();

		char filename[256];
		sprintf(filename, "tests/output_static_mesh_pos%d.ppm", i);
		save_ppm(filename, ref_img, TEST_WIDTH, TEST_HEIGHT, 1.0f);

		// 3. Render TARGET (Billboard)
		g_app.billboard_mode = 1;
		app_render(&g_app);
		float* tgt_img = capture_frame_buffer();

		sprintf(filename, "tests/output_static_billboard_pos%d.ppm", i);
		save_ppm(filename, tgt_img, TEST_WIDTH, TEST_HEIGHT, 1.0f);

		// 4. Compare
		float mse =
		    compute_mse(ref_img, tgt_img, TEST_WIDTH, TEST_HEIGHT);
		printf("[Pos %d] Static MSE: %.6f\n", i, mse);

		if (mse > max_mse_found)
			max_mse_found = mse;
		total_mse += mse;

		// Save Diff Highlighting Errors
		float* diff_img =
		    malloc(TEST_WIDTH * TEST_HEIGHT * 3 * sizeof(float));
		for (int p = 0; p < TEST_WIDTH * TEST_HEIGHT * 3; p++) {
			float d = fabs(ref_img[p] - tgt_img[p]);
			// Highlight significant errors (>5%) in Red
			if (d > 0.05f) {
				diff_img[p] = d;  // R
				if ((p % 3) != 0)
					diff_img[p] =
					    0.0f;  // Mask G/B to make it red
			} else {
				diff_img[p] =
				    d * 10.0f;  // Boost small errors gray
			}
		}
		sprintf(filename, "tests/output_static_diff_pos%d.ppm", i);
		save_ppm(filename, diff_img, TEST_WIDTH, TEST_HEIGHT,
		         1.0f);  // Scale 1.0 as we handled boost manually
		free(diff_img);

		free(ref_img);
		free(tgt_img);
	}

	printf("Total Average MSE: %.6f, Max MSE: %.6f\n",
	       total_mse / num_positions, max_mse_found);
	TEST_ASSERT_FLOAT_WITHIN(MAX_MSE_THRESHOLD, 0.0f, max_mse_found);
}

// Helper: Run Jitter Test and Return Stats
void run_jitter_test(const char* name, int is_billboard)
{
	// 1. Setup Scene
	glm_vec3_copy((vec3){0.0f, 0.0f, 2.5f}, g_app.camera.position);
	g_app.camera.yaw = -90.0f;
	g_app.camera.pitch = 0.0f;
	camera_update_vectors(&g_app.camera);

	g_app.billboard_mode = is_billboard;

	// 2. Render Frame 0
	app_render(&g_app);
	float* frame0 = capture_frame_buffer();
	char filename[256];
	sprintf(filename, "tests/output_jitter_%s_f0.ppm", name);
	save_ppm(filename, frame0, TEST_WIDTH, TEST_HEIGHT, 1.0f);

	// 3. Apply Micro-Jitter
	g_app.camera.position[0] += 0.001f;
	camera_update_vectors(&g_app.camera);

	// 4. Render Frame 1
	app_render(&g_app);
	float* frame1 = capture_frame_buffer();
	sprintf(filename, "tests/output_jitter_%s_f1.ppm", name);
	save_ppm(filename, frame1, TEST_WIDTH, TEST_HEIGHT, 1.0f);

	// 5. Compute Stats
	float avg_diff, max_diff;
	compute_temporal_stats(frame0, frame1, TEST_WIDTH, TEST_HEIGHT,
	                       &avg_diff, &max_diff);

	printf("[%s] Temporal Stability - Avg: %.6f, Max: %.6f\n", name,
	       avg_diff, max_diff);

	// Save Diff
	float* diff_jitter =
	    malloc(TEST_WIDTH * TEST_HEIGHT * 3 * sizeof(float));
	for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT * 3; i++) {
		diff_jitter[i] = fabs(frame0[i] - frame1[i]);
	}
	sprintf(filename, "tests/output_jitter_%s_diff.ppm", name);
	save_ppm(filename, diff_jitter, TEST_WIDTH, TEST_HEIGHT, 5.0f);
	free(diff_jitter);

	// Assert on Avg Diff to catch general instability
	TEST_ASSERT_FLOAT_WITHIN_MESSAGE(MAX_VARIANCE_THRESHOLD, 0.0f, avg_diff,
	                                 "Average jitter too high");

	// Warn if we have large local spikes (fireflies)
	if (max_diff > 0.1f) {
		printf(
		    "WARNING: High local instability detected (Max "
		    "Diff: %.2f) in %s!\n",
		    max_diff, name);
	}

	free(frame0);
	free(frame1);
}

void test_temporal_stability_comparison(void)
{
	printf("Running Mesh Jitter Test (Reference)...\n");
	run_jitter_test("mesh", 0);

	printf("Running Billboard Jitter Test (Target)...\n");
	run_jitter_test("billboard", 1);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_static_fidelity_multi_angle);
	RUN_TEST(test_temporal_stability_comparison);

	if (g_app_initialized) {
		app_cleanup(&g_app);
	}

	return UNITY_END();
}
