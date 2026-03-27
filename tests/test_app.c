#define _POSIX_C_SOURCE 199309L
#include "app.h"
#include "camera.h"
#include "main.h"
#include "postprocess_presets.h"
#include "renderer.h"
#include "scene.h"
#include "unity.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Instance App partagée entre tous les tests
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static App g_test_app;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool g_app_initialized = false;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static GLuint g_cached_hdr_texture = 0;

// PBO for async glReadPixels (double buffering)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static GLuint g_pbo[2] = {0, 0};
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static int g_pbo_index = 0;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static GLsync g_pbo_sync[2] = {NULL, NULL};

static const int POLL_TIMEOUT_ITERATIONS = 1000;
static const long NANOSLEEP_DURATION = 10000000L;
static const int BYTES_PER_PIXEL = 3;
static const float PIXEL_TOLERANCE = 5.0F;
static const float DIFF_PERCENTAGE_TOLERANCE = 0.02F;
static const int DIFF_MAP_VALUE = 255;
static const float PERCENTAGE_FACTOR = 100.0F;
static const int FACTOR_DIV2 = 2;
static const int COORD_DEC = 1;
static const float CAMERA_DIST = 25.0F;
static const float ANGLE_DISPLACEMENT = 2.0F;
static const int PATH_BUF_SIZE = 256;
static const int ERR_BUF_SIZE = 512;
static const uint64_t DEFAULT_SYNC_TIMEOUT = 1000000000ULL;

typedef struct {
	const char* name;
	vec3 pos;
	float yaw;
	float pitch;
	vec3 world_up;
} ViewPoint;

static const ViewPoint G_VIEWPOINTS[] = {
    {"front", {0, 0, CAMERA_DIST}, -90.0F, 0.0F, {0, 1, 0}},
    {"back", {0, 0, -CAMERA_DIST}, 90.0F, 0.0F, {0, 1, 0}},
    {"left", {-CAMERA_DIST, 0, 0}, 0.0F, 0.0F, {0, 1, 0}},
    {"right", {CAMERA_DIST, 0, 0}, 180.0F, 0.0F, {0, 1, 0}},
    {"top", {0, CAMERA_DIST, 0}, -90.0F, -90.0F, {0, 0, -1}},
    {"bottom", {0, -CAMERA_DIST, 0}, -90.0F, 90.0F, {0, 0, 1}}};

static const int NUM_VIEWPOINTS = sizeof(G_VIEWPOINTS) / sizeof(ViewPoint);

void setUp(void)
{
	// Initialiser une seule fois pour tous les tests
	if (!g_app_initialized) {
		int result = app_init(&g_test_app, WINDOW_WIDTH, WINDOW_HEIGHT,
		                      "Integration Test");
		TEST_ASSERT_EQUAL_INT(1, result);
		g_app_initialized = true;

		// Wait for async HDR texture load AND transition to finish
		int timeout = POLL_TIMEOUT_ITERATIONS;
		double last_time = glfwGetTime();
		while (
		    (g_test_app.scene.hdr_texture == 0 ||
		     g_test_app.env_mgr.transition_state != TRANSITION_IDLE) &&
		    timeout-- > 0) {
			double current_time = glfwGetTime();
			g_test_app.delta_time = current_time - last_time;
			last_time = current_time;

			app_update(&g_test_app);
			glfwPollEvents();
			struct timespec req = {0, NANOSLEEP_DURATION};
			nanosleep(&req, NULL);
		}

		// Cache the loaded texture for subsequent tests
		if (g_test_app.scene.hdr_texture != 0) {
			g_cached_hdr_texture = g_test_app.scene.hdr_texture;
		}

		// Initialize PBOs for async pixel readback (optimization#2)
		int fb_width = 0;
		int fb_height = 0;
		glfwGetFramebufferSize(g_test_app.window, &fb_width,
		                       &fb_height);
		size_t pixel_data_size =
		    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);

		glGenBuffers(2, g_pbo);
		for (int i = 0; i < 2; i++) {
			glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[i]);
			glBufferData(GL_PIXEL_PACK_BUFFER,
			             (GLsizeiptr)pixel_data_size, NULL,
			             GL_STREAM_READ);
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	} else if (g_cached_hdr_texture != 0) {
		// Reuse cached texture for subsequent tests
		g_test_app.scene.hdr_texture = g_cached_hdr_texture;
	}
}

void tearDown(void)
{
	// Ne rien faire ici, on cleanup dans main()
}

/**
 * Flips the image vertically (OpenGL reads bottom-to-top, PNG needs
 * top-to-bottom).
 */
static void flip_image_vertically(int image_width, int image_height,
                                  unsigned char* image_data)
{
	int row_sz = image_width * BYTES_PER_PIXEL;
	unsigned char* row_tmp = (unsigned char*)malloc((size_t)row_sz);
	if (!row_tmp) {
		return;
	}

	for (int y_coord = 0; y_coord < (image_height / FACTOR_DIV2);
	     y_coord++) {
		unsigned char* top_row = image_data + (y_coord * row_sz);
		unsigned char* bottom_row =
		    image_data +
		    (((image_height - y_coord) - COORD_DEC) * row_sz);
		(void)memcpy(row_tmp, top_row, (size_t)row_sz);
		(void)memcpy(top_row, bottom_row, (size_t)row_sz);
		(void)memcpy(bottom_row, row_tmp, (size_t)row_sz);
	}
	free(row_tmp);
}

static void verify_reference_image(int width, int height,
                                   unsigned char* current_pixels,
                                   const char* face_name)
{
	size_t pixel_data_size = (size_t)(width * height * BYTES_PER_PIXEL);

	char ref_path[PATH_BUF_SIZE];
	(void)snprintf(ref_path, sizeof(ref_path),
	               "tests/references/ref_%s.png", face_name);

	// Load reference frame (PNG)
	int ref_w = 0;
	int ref_h = 0;
	int ref_channels = 0;
	unsigned char* ref_pixels =
	    stbi_load(ref_path, &ref_w, &ref_h, &ref_channels, BYTES_PER_PIXEL);
	if (ref_pixels == NULL) {
		char err_msg[ERR_BUF_SIZE];
		(void)snprintf(err_msg, sizeof(err_msg),
		               "Reference image %s not found.", ref_path);
		TEST_FAIL_MESSAGE(err_msg);
		return;
	}

	if (ref_w != width || ref_h != height) {
		stbi_image_free(ref_pixels);
		TEST_FAIL_MESSAGE("Reference image dimensions mismatch");
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

	if (diff_percentage > DIFF_PERCENTAGE_TOLERANCE) {
		// Visual debug output only on failure
		unsigned char* diff_map =
		    (unsigned char*)malloc(pixel_data_size);
		if (diff_map) {
			for (size_t i = 0; i < pixel_data_size; i++) {
				int delta = abs((int)current_pixels[i] -
				                (int)ref_pixels[i]);
				diff_map[i] = (delta > (int)PIXEL_TOLERANCE)
				                  ? DIFF_MAP_VALUE
				                  : 0;
			}

			char diff_path[PATH_BUF_SIZE];
			(void)snprintf(diff_path, sizeof(diff_path),
			               "tests/references/failed_diff_%s.png",
			               face_name);
			(void)stbi_write_png(diff_path, width, height,
			                     BYTES_PER_PIXEL, diff_map,
			                     width * BYTES_PER_PIXEL);
			free(diff_map);
		}

		char actual_path[PATH_BUF_SIZE];
		(void)snprintf(actual_path, sizeof(actual_path),
		               "tests/references/failed_actual_%s.png",
		               face_name);
		(void)stbi_write_png(actual_path, width, height,
		                     BYTES_PER_PIXEL, current_pixels,
		                     width * BYTES_PER_PIXEL);

		printf(
		    "\n[VISUAL] Regression detected on face %s! Diff: %.2f%%\n",
		    face_name, (double)(diff_percentage * PERCENTAGE_FACTOR));
	}

	stbi_image_free(ref_pixels);

	// Allow up to 2% difference for MSAA/driver noise
	TEST_ASSERT_FLOAT_WITHIN(DIFF_PERCENTAGE_TOLERANCE, 0.0F,
	                         diff_percentage);
}

/**
 * Integration Test: Full lifecycle and single frame rendering validation
 */
static void test_app_render_multi_view(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	// Get actual framebuffer dimensions
	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);

	// Texture should already be loaded and cached from setUp()
	TEST_ASSERT_NOT_EQUAL_MESSAGE(0, g_test_app.scene.hdr_texture,
	                              "HDR texture never loaded");

	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* current_pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(current_pixels);

	int capture_mode = (getenv("GEN_REFS") != NULL);

	for (int i = 0; i < NUM_VIEWPOINTS; i++) {
		const ViewPoint* vpoint = &G_VIEWPOINTS[i];
		printf("[INFO] Testing viewpoint: %s\n", vpoint->name);

		// Set camera
		glm_vec3_copy(
		    (vec3){vpoint->pos[0], vpoint->pos[1], vpoint->pos[2]},
		    g_test_app.camera.position);
		glm_vec3_copy((vec3){vpoint->world_up[0], vpoint->world_up[1],
		                     vpoint->world_up[2]},
		              g_test_app.camera.world_up);
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;
		camera_update_vectors(&g_test_app.camera);

		// Render
		renderer_draw_frame(
		    &g_test_app, &g_test_app.scene, &g_test_app.postprocess,
		    &g_test_app.camera, &g_test_app.gpu_profiler,
		    &g_test_app.timeline_ui, &g_test_app.env_mgr,
		    &g_test_app.notifier, &g_test_app.effect_bench,
		    g_test_app.width, g_test_app.height, g_test_app.delta_time,
		    g_test_app.frame_count, g_test_app.log_gpu_metrics);

		// Async capture using PBO (optimization#2)
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		// Bind PBO for async DMA transfer
		int current_pbo = g_pbo_index;
		int next_pbo = (g_pbo_index + 1) % 2;

		// Start async read to PBO
		glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[current_pbo]);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, 0);

		// Fence the read
		if (g_pbo_sync[current_pbo]) {
			glDeleteSync(g_pbo_sync[current_pbo]);
		}
		g_pbo_sync[current_pbo] =
		    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		// Process first frame immediately (no previous frame)
		if (i == 0) {
			// Must wait for current fence
			glClientWaitSync(g_pbo_sync[current_pbo],
			                 GL_SYNC_FLUSH_COMMANDS_BIT,
			                 DEFAULT_SYNC_TIMEOUT);
			void* mapped =
			    glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			if (mapped) {
				(void)memcpy(current_pixels, mapped,
				             pixel_data_size);
				glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

				// Flip for PNG/comparison
				flip_image_vertically(fb_width, fb_height,
				                      current_pixels);

				if (capture_mode) {
					char ref_path[PATH_BUF_SIZE];
					(void)snprintf(
					    ref_path, sizeof(ref_path),
					    "tests/references/ref_%s.png",
					    vpoint->name);
					(void)stbi_write_png(
					    ref_path, fb_width, fb_height,
					    BYTES_PER_PIXEL, current_pixels,
					    fb_width * BYTES_PER_PIXEL);
					printf(
					    "[INFO] Reference generated: %s\n",
					    ref_path);
				} else {
					verify_reference_image(
					    fb_width, fb_height, current_pixels,
					    vpoint->name);
				}
			}
		}

		// Map previous frame's PBO (which should now be ready)
		if (i > 0) {
			// Wait for previous fence
			if (g_pbo_sync[next_pbo]) {
				glClientWaitSync(g_pbo_sync[next_pbo],
				                 GL_SYNC_FLUSH_COMMANDS_BIT,
				                 DEFAULT_SYNC_TIMEOUT);
			}
			glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[next_pbo]);
			void* mapped =
			    glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			if (mapped) {
				(void)memcpy(current_pixels, mapped,
				             pixel_data_size);
				glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

				// Flip for PNG/comparison
				flip_image_vertically(fb_width, fb_height,
				                      current_pixels);

				// Verify/generate for the PREVIOUS viewpoint
				// (i-1)
				const ViewPoint* prev_vpoint =
				    &G_VIEWPOINTS[i - 1];
				if (capture_mode) {
					char ref_path[PATH_BUF_SIZE];
					(void)snprintf(
					    ref_path, sizeof(ref_path),
					    "tests/references/ref_%s.png",
					    prev_vpoint->name);
					(void)stbi_write_png(
					    ref_path, fb_width, fb_height,
					    BYTES_PER_PIXEL, current_pixels,
					    fb_width * BYTES_PER_PIXEL);
					printf(
					    "[INFO] Reference generated: %s\n",
					    ref_path);
				} else {
					// Verify previous viewpoint
					verify_reference_image(
					    fb_width, fb_height, current_pixels,
					    prev_vpoint->name);
				}
			}
		}

		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		g_pbo_index = next_pbo;
	}

	// Read the last frame from the PBO (we're one frame behind)
	if (NUM_VIEWPOINTS > 0) {
		int last_pbo = (g_pbo_index + 1) % 2;
		if (g_pbo_sync[last_pbo]) {
			glClientWaitSync(g_pbo_sync[last_pbo],
			                 GL_SYNC_FLUSH_COMMANDS_BIT,
			                 DEFAULT_SYNC_TIMEOUT);
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[last_pbo]);
		void* mapped = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		if (mapped) {
			(void)memcpy(current_pixels, mapped, pixel_data_size);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

			// Flip for PNG/comparison
			flip_image_vertically(fb_width, fb_height,
			                      current_pixels);

			const ViewPoint* last_vpoint =
			    &G_VIEWPOINTS[NUM_VIEWPOINTS - 1];
			if (capture_mode) {
				char ref_path[PATH_BUF_SIZE];
				(void)snprintf(ref_path, sizeof(ref_path),
				               "tests/references/ref_%s.png",
				               last_vpoint->name);
				(void)stbi_write_png(
				    ref_path, fb_width, fb_height,
				    BYTES_PER_PIXEL, current_pixels,
				    fb_width * BYTES_PER_PIXEL);
				printf("[INFO] Reference generated: %s\n",
				       ref_path);
			} else {
				// Verify last viewpoint
				verify_reference_image(fb_width, fb_height,
				                       current_pixels,
				                       last_vpoint->name);
			}
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	}

	free(current_pixels);
}

/**
 * Test Subtle Mode with Bloom
 */
static void test_app_render_subtle_bloom(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);

	// Apply Subtle Preset + Bloom
	postprocess_apply_preset(&g_test_app.postprocess, &PRESET_SUBTLE);
	postprocess_enable(&g_test_app.postprocess, POSTFX_BLOOM);

	// Disable Vignette and Grain as requested
	postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);

	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(pixels);

	for (int i = 0; i < NUM_VIEWPOINTS; i++) {
		const ViewPoint* vpoint = &G_VIEWPOINTS[i];
		printf("[INFO] Testing Subtle Bloom viewpoint: %s\n",
		       vpoint->name);

		// Set camera
		glm_vec3_copy(
		    (vec3){vpoint->pos[0], vpoint->pos[1], vpoint->pos[2]},
		    g_test_app.camera.position);
		glm_vec3_copy((vec3){vpoint->world_up[0], vpoint->world_up[1],
		                     vpoint->world_up[2]},
		              g_test_app.camera.world_up);
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;
		camera_update_vectors(&g_test_app.camera);

		// Force one frame update to sync UBO
		app_update(&g_test_app);

		// Render
		renderer_draw_frame(
		    &g_test_app, &g_test_app.scene, &g_test_app.postprocess,
		    &g_test_app.camera, &g_test_app.gpu_profiler,
		    &g_test_app.timeline_ui, &g_test_app.env_mgr,
		    &g_test_app.notifier, &g_test_app.effect_bench,
		    g_test_app.width, g_test_app.height, g_test_app.delta_time,
		    g_test_app.frame_count, g_test_app.log_gpu_metrics);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, pixels);
		flip_image_vertically(fb_width, fb_height, pixels);

		char test_name[PATH_BUF_SIZE];
		(void)snprintf(test_name, sizeof(test_name), "%s_subtle_bloom",
		               vpoint->name);

		if (getenv("GEN_REFS") != NULL) {
			char ref_path[PATH_BUF_SIZE];
			(void)snprintf(ref_path, sizeof(ref_path),
			               "tests/references/ref_%s.png",
			               test_name);
			(void)stbi_write_png(ref_path, fb_width, fb_height,
			                     BYTES_PER_PIXEL, pixels,
			                     fb_width * BYTES_PER_PIXEL);
			printf("[INFO] Reference generated: %s\n", ref_path);
		} else {
			verify_reference_image(fb_width, fb_height, pixels,
			                       test_name);
		}
	}

	free(pixels);
}

/**
 * Test camera initialization
 */
static void test_app_camera_initialization(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	// Verify camera is properly initialized
	TEST_ASSERT_GREATER_THAN_FLOAT(0.0F, g_test_app.camera.zoom);
}

/**
 * Test Auto Exposure
 */
/**
 * Test Subtle Mode with Auto Exposure
 */
static void test_app_render_subtle_auto_exposure(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);

	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(pixels);

	for (int i = 0; i < NUM_VIEWPOINTS; i++) {
		const ViewPoint* vpoint = &G_VIEWPOINTS[i];
		printf("[INFO] Testing Subtle Auto Exposure viewpoint: %s\n",
		       vpoint->name);

		// Set camera
		glm_vec3_copy(
		    (vec3){vpoint->pos[0], vpoint->pos[1], vpoint->pos[2]},
		    g_test_app.camera.position);
		glm_vec3_copy((vec3){vpoint->world_up[0], vpoint->world_up[1],
		                     vpoint->world_up[2]},
		              g_test_app.camera.world_up);
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;
		camera_update_vectors(&g_test_app.camera);

		// Apply Subtle Preset + Auto Exposure
		postprocess_apply_preset(&g_test_app.postprocess,
		                         &PRESET_SUBTLE);
		postprocess_enable(&g_test_app.postprocess,
		                   POSTFX_AUTO_EXPOSURE);

		// Disable Vignette and Grain as requested
		postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
		postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);

		// Warmup auto-exposure (128 frames)
		const int warmup_frames = 128;
		for (int frame = 0; frame < warmup_frames; frame++) {
			app_update(&g_test_app);
			renderer_draw_frame(
			    &g_test_app, &g_test_app.scene,
			    &g_test_app.postprocess, &g_test_app.camera,
			    &g_test_app.gpu_profiler, &g_test_app.timeline_ui,
			    &g_test_app.env_mgr, &g_test_app.notifier,
			    &g_test_app.effect_bench, g_test_app.width,
			    g_test_app.height, g_test_app.delta_time,
			    g_test_app.frame_count, g_test_app.log_gpu_metrics);
		}

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, pixels);
		flip_image_vertically(fb_width, fb_height, pixels);

		char test_name[PATH_BUF_SIZE];
		(void)snprintf(test_name, sizeof(test_name),
		               "%s_subtle_auto_exposure", vpoint->name);

		if (getenv("GEN_REFS") != NULL) {
			char ref_path[PATH_BUF_SIZE];
			(void)snprintf(ref_path, sizeof(ref_path),
			               "tests/references/ref_%s.png",
			               test_name);
			(void)stbi_write_png(ref_path, fb_width, fb_height,
			                     BYTES_PER_PIXEL, pixels,
			                     fb_width * BYTES_PER_PIXEL);
			printf("[INFO] Reference generated: %s\n", ref_path);
		} else {
			verify_reference_image(fb_width, fb_height, pixels,
			                       test_name);
		}
	}

	free(pixels);
}

/**
 * Test FXAA
 */
/**
 * Test Subtle Mode with FXAA
 */
static void test_app_render_subtle_fxaa(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);

	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(pixels);

	for (int i = 0; i < NUM_VIEWPOINTS; i++) {
		const ViewPoint* vpoint = &G_VIEWPOINTS[i];
		printf("[INFO] Testing Subtle FXAA viewpoint: %s\n",
		       vpoint->name);

		// Set camera
		glm_vec3_copy(
		    (vec3){vpoint->pos[0], vpoint->pos[1], vpoint->pos[2]},
		    g_test_app.camera.position);
		glm_vec3_copy((vec3){vpoint->world_up[0], vpoint->world_up[1],
		                     vpoint->world_up[2]},
		              g_test_app.camera.world_up);
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;
		camera_update_vectors(&g_test_app.camera);

		// Apply Subtle Preset + FXAA
		postprocess_apply_preset(&g_test_app.postprocess,
		                         &PRESET_SUBTLE);
		postprocess_enable(&g_test_app.postprocess, POSTFX_FXAA);

		// Disable Vignette and Grain as requested
		postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
		postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);

		// Force one frame update to sync UBO
		app_update(&g_test_app);

		// Render
		renderer_draw_frame(
		    &g_test_app, &g_test_app.scene, &g_test_app.postprocess,
		    &g_test_app.camera, &g_test_app.gpu_profiler,
		    &g_test_app.timeline_ui, &g_test_app.env_mgr,
		    &g_test_app.notifier, &g_test_app.effect_bench,
		    g_test_app.width, g_test_app.height, g_test_app.delta_time,
		    g_test_app.frame_count, g_test_app.log_gpu_metrics);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, pixels);
		flip_image_vertically(fb_width, fb_height, pixels);

		char test_name[PATH_BUF_SIZE];
		(void)snprintf(test_name, sizeof(test_name), "%s_subtle_fxaa",
		               vpoint->name);

		if (getenv("GEN_REFS") != NULL) {
			char ref_path[PATH_BUF_SIZE];
			(void)snprintf(ref_path, sizeof(ref_path),
			               "tests/references/ref_%s.png",
			               test_name);
			(void)stbi_write_png(ref_path, fb_width, fb_height,
			                     BYTES_PER_PIXEL, pixels,
			                     fb_width * BYTES_PER_PIXEL);
			printf("[INFO] Reference generated: %s\n", ref_path);
		} else {
			verify_reference_image(fb_width, fb_height, pixels,
			                       test_name);
		}
	}

	free(pixels);
}

/**
 * Test Subtle Mode without additional effects
 */
static void test_app_render_subtle_none(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);

	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(pixels);

	for (int i = 0; i < NUM_VIEWPOINTS; i++) {
		const ViewPoint* vpoint = &G_VIEWPOINTS[i];
		printf("[INFO] Testing Subtle None viewpoint: %s\n",
		       vpoint->name);

		// Set camera
		glm_vec3_copy(
		    (vec3){vpoint->pos[0], vpoint->pos[1], vpoint->pos[2]},
		    g_test_app.camera.position);
		glm_vec3_copy((vec3){vpoint->world_up[0], vpoint->world_up[1],
		                     vpoint->world_up[2]},
		              g_test_app.camera.world_up);
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;
		camera_update_vectors(&g_test_app.camera);

		// Apply Subtle Preset
		postprocess_apply_preset(&g_test_app.postprocess,
		                         &PRESET_SUBTLE);

		// Disable Vignette and Grain as requested
		postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
		postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);

		// Force one frame update to sync UBO
		app_update(&g_test_app);

		// Render
		renderer_draw_frame(
		    &g_test_app, &g_test_app.scene, &g_test_app.postprocess,
		    &g_test_app.camera, &g_test_app.gpu_profiler,
		    &g_test_app.timeline_ui, &g_test_app.env_mgr,
		    &g_test_app.notifier, &g_test_app.effect_bench,
		    g_test_app.width, g_test_app.height, g_test_app.delta_time,
		    g_test_app.frame_count, g_test_app.log_gpu_metrics);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, pixels);
		flip_image_vertically(fb_width, fb_height, pixels);

		char test_name[PATH_BUF_SIZE];
		(void)snprintf(test_name, sizeof(test_name), "%s_subtle_none",
		               vpoint->name);

		if (getenv("GEN_REFS") != NULL) {
			char ref_path[PATH_BUF_SIZE];
			(void)snprintf(ref_path, sizeof(ref_path),
			               "tests/references/ref_%s.png",
			               test_name);
			(void)stbi_write_png(ref_path, fb_width, fb_height,
			                     BYTES_PER_PIXEL, pixels,
			                     fb_width * BYTES_PER_PIXEL);
			printf("[INFO] Reference generated: %s\n", ref_path);
		} else {
			verify_reference_image(fb_width, fb_height, pixels,
			                       test_name);
		}
	}

	free(pixels);
}

/**
 * Test Depth of Field (DoF)
 */
/**
 * Test Subtle Mode with Depth of Field (DoF)
 */
static void test_app_render_subtle_dof(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);

	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(pixels);

	for (int i = 0; i < NUM_VIEWPOINTS; i++) {
		const ViewPoint* vpoint = &G_VIEWPOINTS[i];
		printf("[INFO] Testing Subtle DoF viewpoint: %s\n",
		       vpoint->name);

		// Set camera
		glm_vec3_copy(
		    (vec3){vpoint->pos[0], vpoint->pos[1], vpoint->pos[2]},
		    g_test_app.camera.position);
		glm_vec3_copy((vec3){vpoint->world_up[0], vpoint->world_up[1],
		                     vpoint->world_up[2]},
		              g_test_app.camera.world_up);
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;
		camera_update_vectors(&g_test_app.camera);

		// Apply Subtle Preset + DoF
		postprocess_apply_preset(&g_test_app.postprocess,
		                         &PRESET_SUBTLE);
		postprocess_enable(&g_test_app.postprocess, POSTFX_DOF);

		// Disable Vignette and Grain as requested
		postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
		postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);

		// Force one frame update to sync UBO/matrices
		app_update(&g_test_app);

		// Render
		renderer_draw_frame(
		    &g_test_app, &g_test_app.scene, &g_test_app.postprocess,
		    &g_test_app.camera, &g_test_app.gpu_profiler,
		    &g_test_app.timeline_ui, &g_test_app.env_mgr,
		    &g_test_app.notifier, &g_test_app.effect_bench,
		    g_test_app.width, g_test_app.height, g_test_app.delta_time,
		    g_test_app.frame_count, g_test_app.log_gpu_metrics);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, pixels);
		flip_image_vertically(fb_width, fb_height, pixels);

		char test_name[PATH_BUF_SIZE];
		(void)snprintf(test_name, sizeof(test_name), "%s_subtle_dof",
		               vpoint->name);

		if (getenv("GEN_REFS") != NULL) {
			char ref_path[PATH_BUF_SIZE];
			(void)snprintf(ref_path, sizeof(ref_path),
			               "tests/references/ref_%s.png",
			               test_name);
			(void)stbi_write_png(ref_path, fb_width, fb_height,
			                     BYTES_PER_PIXEL, pixels,
			                     fb_width * BYTES_PER_PIXEL);
			printf("[INFO] Reference generated: %s\n", ref_path);
		} else {
			verify_reference_image(fb_width, fb_height, pixels,
			                       test_name);
		}
	}

	free(pixels);
}

/**
 * Test Subtle Mode with Motion Blur
 * Uses a double-frame sequence to generate deterministic blur.
 */
static void test_app_render_subtle_motion_blur(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);

	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(pixels);

	for (int i = 0; i < NUM_VIEWPOINTS; i++) {
		const ViewPoint* vpoint = &G_VIEWPOINTS[i];
		printf("[INFO] Testing Subtle Motion Blur viewpoint: %s\n",
		       vpoint->name);

		// Apply Subtle Preset + Motion Blur
		postprocess_apply_preset(&g_test_app.postprocess,
		                         &PRESET_SUBTLE);
		postprocess_enable(&g_test_app.postprocess, POSTFX_MOTION_BLUR);

		// Disable Vignette and Grain as requested
		postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
		postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);

		// --- FRAME 1: WARMUP (SET PREVIOUS_VIEW_PROJ) ---
		glm_vec3_copy(
		    (vec3){vpoint->pos[0], vpoint->pos[1], vpoint->pos[2]},
		    g_test_app.camera.position);
		glm_vec3_copy((vec3){vpoint->world_up[0], vpoint->world_up[1],
		                     vpoint->world_up[2]},
		              g_test_app.camera.world_up);
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;

		// Apply NEGATIVE view-dependent deterministic movement for the
		// WARMUP frame This ensures Frame 2 (the captured frame) lands
		// exactly on the standard view position, making it perfectly
		// comparable to the "none" reference image in the Diff Map.
		float angle_disp = ANGLE_DISPLACEMENT;
		if (strcmp(vpoint->name, "front") == 0) {
			g_test_app.camera.yaw -= angle_disp;
		} else if (strcmp(vpoint->name, "back") == 0) {
			g_test_app.camera.yaw += angle_disp;
		} else if (strcmp(vpoint->name, "left") == 0) {
			g_test_app.camera.pitch -= angle_disp;
		} else if (strcmp(vpoint->name, "right") == 0) {
			g_test_app.camera.pitch += angle_disp;
		} else if (strcmp(vpoint->name, "top") == 0) {
			g_test_app.camera.pitch -= angle_disp;
		} else if (strcmp(vpoint->name, "bottom") == 0) {
			g_test_app.camera.pitch += angle_disp;
		}
		camera_update_vectors(&g_test_app.camera);

		app_update(&g_test_app);
		renderer_draw_frame(
		    &g_test_app, &g_test_app.scene, &g_test_app.postprocess,
		    &g_test_app.camera, &g_test_app.gpu_profiler,
		    &g_test_app.timeline_ui, &g_test_app.env_mgr,
		    &g_test_app.notifier, &g_test_app.effect_bench,
		    g_test_app.width, g_test_app.height, g_test_app.delta_time,
		    g_test_app.frame_count, g_test_app.log_gpu_metrics);

		// --- FRAME 2: MOTION (CAPTURED) ---
		// Restore camera to the exact baseline view point for the
		// capture. The movement FROM Frame 1 TO Frame 2 will create the
		// velocity vectors.
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;
		camera_update_vectors(&g_test_app.camera);

		app_update(&g_test_app);
		renderer_draw_frame(
		    &g_test_app, &g_test_app.scene, &g_test_app.postprocess,
		    &g_test_app.camera, &g_test_app.gpu_profiler,
		    &g_test_app.timeline_ui, &g_test_app.env_mgr,
		    &g_test_app.notifier, &g_test_app.effect_bench,
		    g_test_app.width, g_test_app.height, g_test_app.delta_time,
		    g_test_app.frame_count, g_test_app.log_gpu_metrics);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, pixels);
		flip_image_vertically(fb_width, fb_height, pixels);

		char test_name[PATH_BUF_SIZE];
		(void)snprintf(test_name, sizeof(test_name),
		               "%s_subtle_motion_blur", vpoint->name);

		if (getenv("GEN_REFS") != NULL) {
			char ref_path[PATH_BUF_SIZE];
			(void)snprintf(ref_path, sizeof(ref_path),
			               "tests/references/ref_%s.png",
			               test_name);
			(void)stbi_write_png(ref_path, fb_width, fb_height,
			                     BYTES_PER_PIXEL, pixels,
			                     fb_width * BYTES_PER_PIXEL);
			printf("[INFO] Reference generated: %s\n", ref_path);
		} else {
			verify_reference_image(fb_width, fb_height, pixels,
			                       test_name);
		}
	}

	free(pixels);
}

/**
 * Test Sony A7S III Preset (Anamorphic DoF + 3D LUT)
 */
static void test_app_render_sony_a7siii(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");

	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);

	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);
	unsigned char* pixels = (unsigned char*)malloc(pixel_data_size);
	TEST_ASSERT_NOT_NULL(pixels);

	for (int i = 0; i < NUM_VIEWPOINTS; i++) {
		const ViewPoint* vpoint = &G_VIEWPOINTS[i];
		printf("[INFO] Testing Sony A7S III viewpoint: %s\n",
		       vpoint->name);

		// Set camera
		glm_vec3_copy(
		    (vec3){vpoint->pos[0], vpoint->pos[1], vpoint->pos[2]},
		    g_test_app.camera.position);
		glm_vec3_copy((vec3){vpoint->world_up[0], vpoint->world_up[1],
		                     vpoint->world_up[2]},
		              g_test_app.camera.world_up);
		g_test_app.camera.yaw = vpoint->yaw;
		g_test_app.camera.pitch = vpoint->pitch;
		camera_update_vectors(&g_test_app.camera);

		// Apply Sony A7S III Preset + load S-Cinetone 3D LUT
		postprocess_apply_preset(&g_test_app.postprocess,
		                         &PRESET_SONY_A7SIII);
		postprocess_load_lut3d(&g_test_app.postprocess,
		                       "assets/luts/sony_scinetone.cube");

		// Warmup auto-exposure (128 frames) — Sony preset uses
		// POSTFX_AUTO_EXPOSURE which needs convergence time
		const int warmup_frames = 128;
		for (int frame = 0; frame < warmup_frames; frame++) {
			app_update(&g_test_app);
			renderer_draw_frame(
			    &g_test_app, &g_test_app.scene,
			    &g_test_app.postprocess, &g_test_app.camera,
			    &g_test_app.gpu_profiler, &g_test_app.timeline_ui,
			    &g_test_app.env_mgr, &g_test_app.notifier,
			    &g_test_app.effect_bench, g_test_app.width,
			    g_test_app.height, g_test_app.delta_time,
			    g_test_app.frame_count, g_test_app.log_gpu_metrics);
		}

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, pixels);
		flip_image_vertically(fb_width, fb_height, pixels);

		char test_name[PATH_BUF_SIZE];
		(void)snprintf(test_name, sizeof(test_name), "%s_sony_a7siii",
		               vpoint->name);

		if (getenv("GEN_REFS") != NULL) {
			char ref_path[PATH_BUF_SIZE];
			(void)snprintf(ref_path, sizeof(ref_path),
			               "tests/references/ref_%s.png",
			               test_name);
			(void)stbi_write_png(ref_path, fb_width, fb_height,
			                     BYTES_PER_PIXEL, pixels,
			                     fb_width * BYTES_PER_PIXEL);
			printf("[INFO] Reference generated: %s\n", ref_path);
		} else {
			verify_reference_image(fb_width, fb_height, pixels,
			                       test_name);
		}
	}

	free(pixels);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_app_render_multi_view);
	RUN_TEST(test_app_render_subtle_none);
	RUN_TEST(test_app_render_subtle_bloom);
	RUN_TEST(test_app_render_subtle_auto_exposure);
	RUN_TEST(test_app_render_subtle_fxaa);
	RUN_TEST(test_app_render_subtle_dof);
	RUN_TEST(test_app_render_subtle_motion_blur);
	RUN_TEST(test_app_render_sony_a7siii);
	RUN_TEST(test_app_camera_initialization);

	// Cleanup APRÈS tous les tests
	if (g_app_initialized) {
		// Cleanup PBOs
		if (g_pbo[0] != 0) {
			glDeleteBuffers(2, g_pbo);
			g_pbo[0] = 0;
			g_pbo[1] = 0;
		}
		for (int i = 0; i < 2; i++) {
			if (g_pbo_sync[i]) {
				glDeleteSync(g_pbo_sync[i]);
				g_pbo_sync[i] = NULL;
			}
		}
		app_cleanup(&g_test_app);
	}

	return UNITY_END();
}
