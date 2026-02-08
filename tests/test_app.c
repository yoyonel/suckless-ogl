#define _POSIX_C_SOURCE 199309L
#include "app.h"
#include "app_scene.h"
#include "main.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Instance App partagée entre tous les tests
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static App g_test_app;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool g_app_initialized = false;

static const int POLL_TIMEOUT_ITERATIONS = 1000;
static const long NANOSLEEP_DURATION = 10000000L;
static const int BYTES_PER_PIXEL = 3;
static const float PIXEL_TOLERANCE = 5.0F;
static const float DIFF_PERCENTAGE_TOLERANCE = 0.02F;
static const int DIFF_MAP_VALUE = 255;
static const float PERCENTAGE_FACTOR = 100.0F;

void setUp(void)
{
	// Initialiser une seule fois pour tous les tests
	if (!g_app_initialized) {
		int result = app_init(&g_test_app, WINDOW_WIDTH, WINDOW_HEIGHT,
		                      "Integration Test");
		TEST_ASSERT_EQUAL_INT(1, result);
		g_app_initialized = true;
	}
}

void tearDown(void)
{
	// Ne rien faire ici, on cleanup dans main()
}

static void verify_reference_image(int width, int height,
                                   unsigned char* current_pixels)
{
	size_t pixel_data_size = (size_t)(width * height * BYTES_PER_PIXEL);

	// Load reference frame
	FILE* fref = fopen("tests/ref_frame.raw", "rb");
	if (fref == NULL) {
		TEST_FAIL_MESSAGE(
		    "Reference image tests/ref_frame.raw not found.");
		return;  // Redundant if fail aborts, but safe
	}

	unsigned char* ref_pixels = (unsigned char*)malloc(pixel_data_size);
	if (ref_pixels == NULL) {
		fclose(fref);
		TEST_FAIL_MESSAGE("Failed to allocate memory for ref_pixels");
		return;
	}

	size_t read_bytes = fread(ref_pixels, 1, pixel_data_size, fref);
	fclose(fref);
	if (read_bytes != pixel_data_size) {
		free(ref_pixels);
		TEST_ASSERT_EQUAL_UINT_MESSAGE(pixel_data_size, read_bytes,
		                               "Reference file size mismatch");
		return;
	}

	// Compare with tolerance
	int diff_count = 0;

	for (size_t i = 0; i < pixel_data_size; i += BYTES_PER_PIXEL) {
		float diff_r =
		    (float)current_pixels[i + 0] - (float)ref_pixels[i + 0];
		float diff_g =
		    (float)current_pixels[i + 1] - (float)ref_pixels[i + 1];
		float diff_b =
		    (float)current_pixels[i + 2] - (float)ref_pixels[i + 2];
		float dist = sqrtf((diff_r * diff_r) + (diff_g * diff_g) +
		                   (diff_b * diff_b));
		if (dist > PIXEL_TOLERANCE) {
			diff_count++;
		}
	}
	float diff_percentage = (float)diff_count / (float)(width * height);

	// Visual debug output
	unsigned char* diff_map = (unsigned char*)malloc(pixel_data_size);
	if (diff_map) {
		for (size_t i = 0; i < pixel_data_size; i++) {
			int delta =
			    abs((int)current_pixels[i] - (int)ref_pixels[i]);
			diff_map[i] =
			    (delta > (int)PIXEL_TOLERANCE) ? DIFF_MAP_VALUE : 0;
		}

		FILE* fdiff = fopen("tests/failed_diff_map.raw", "wb");
		if (fdiff) {
			(void)fwrite(diff_map, 1, pixel_data_size, fdiff);
			(void)fclose(fdiff);
		}
		free(diff_map);
	}

	FILE* fcur = fopen("tests/failed_frame_actual.raw", "wb");
	if (fcur) {
		(void)fwrite(current_pixels, 1, pixel_data_size, fcur);
		(void)fclose(fcur);
	}

	if (diff_percentage > 0.00F) {
		printf(
		    "\n[VISUAL] Regression detected! Diff: %.2f%% (saved to "
		    "tests/failed_diff_map.raw)\n",
		    (double)(diff_percentage * PERCENTAGE_FACTOR));
	}

	free(ref_pixels);

	// Allow up to 2% difference for MSAA/driver noise
	TEST_ASSERT_FLOAT_WITHIN(DIFF_PERCENTAGE_TOLERANCE, 0.0F,
	                         diff_percentage);
}

/**
 * Integration Test: Full lifecycle and single frame rendering validation
 */
void test_app_render_single_frame(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	// Get actual framebuffer dimensions
	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);
	printf("Resolution: %dx%d\n", fb_width, fb_height);

	// Wait for async load to complete
	printf("Waiting for async HDR load...\n");
	int timeout =
	    POLL_TIMEOUT_ITERATIONS;  // 10s approximately (100 * 100ms)
	                              // -- wait, no loop sleep here, just
	                              // yield
	while (g_test_app.hdr_texture == 0 && timeout-- > 0) {
		app_update(&g_test_app);
		glfwPollEvents();
		struct timespec req = {0, NANOSLEEP_DURATION};  // 10ms
		nanosleep(&req, NULL);
	}
	TEST_ASSERT_NOT_EQUAL_MESSAGE(0, g_test_app.hdr_texture,
	                              "HDR texture never loaded");
	printf("HDR Load completed.\n");

	// Generate geometry and render
	icosphere_generate(&g_test_app.geometry, g_test_app.subdivisions);
	app_update_gpu_buffers(&g_test_app);
	app_render(&g_test_app);

	// Capture current frame pixels
	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* current_pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(current_pixels);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, fb_width, fb_height, GL_RGB, GL_UNSIGNED_BYTE,
	             current_pixels);

	// Verify against ref
	verify_reference_image(fb_width, fb_height, current_pixels);

	free(current_pixels);
}

/**
 * Test camera initialization
 */
void test_app_camera_initialization(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	// Verify camera is properly initialized
	TEST_ASSERT_GREATER_THAN_FLOAT(0.0F, g_test_app.camera.zoom);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_app_render_single_frame);
	RUN_TEST(test_app_camera_initialization);

	// Cleanup APRÈS tous les tests
	if (g_app_initialized) {
		app_cleanup(&g_test_app);
	}

	return UNITY_END();
}
