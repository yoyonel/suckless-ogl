#define _POSIX_C_SOURCE 200809L
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
#define PATH_BUF_SIZE 256
#define ERR_BUF_SIZE 512
#define DEFAULT_SYNC_TIMEOUT 1000000000ULL

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

typedef struct {
	char name[PATH_BUF_SIZE];
	unsigned char* data;
	int width;
	int height;
} CachedRef;

#define MAX_CACHED_REFS 128
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static CachedRef g_ref_cache[MAX_CACHED_REFS];
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static int g_ref_cache_count = 0;

static void preload_reference(const char* test_name)
{
	if (g_ref_cache_count >= MAX_CACHED_REFS) {
		return;
	}

	char ref_path[PATH_BUF_SIZE];
	(void)snprintf(ref_path, sizeof(ref_path), "tests/ref_%s.png",
	               test_name);

	int img_width = 0;
	int img_height = 0;
	int channels = 0;
	unsigned char* data =
	    stbi_load(ref_path, &img_width, &img_height, &channels, 0);
	if (data) {
		(void)strncpy(g_ref_cache[g_ref_cache_count].name, test_name,
		              PATH_BUF_SIZE - 1);
		g_ref_cache[g_ref_cache_count].data = data;
		g_ref_cache[g_ref_cache_count].width = img_width;
		g_ref_cache[g_ref_cache_count].height = img_height;
		g_ref_cache_count++;
	}
}

static void preload_all_references(void)
{
	const int num_test_cases = 6;
	const char* test_cases[] = {"bloom", "auto_exposure", "fxaa",
	                            "none",  "dof",           "motion_blur"};
	for (int case_idx = 0; case_idx < num_test_cases; case_idx++) {
		for (int vp_idx = 0; vp_idx < NUM_VIEWPOINTS; vp_idx++) {
			char full_name[PATH_BUF_SIZE];
			(void)snprintf(
			    full_name, sizeof(full_name), "%s_subtle_%s",
			    G_VIEWPOINTS[vp_idx].name, test_cases[case_idx]);
			preload_reference(full_name);
		}
	}
	// Also preload multi-view faces
	const int num_faces = 6;
	const char* faces[] = {"front", "back", "left",
	                       "right", "top",  "bottom"};
	for (int face_idx = 0; face_idx < num_faces; face_idx++) {
		preload_reference(faces[face_idx]);
	}
}

static CachedRef* get_cached_reference(const char* test_name)
{
	for (int i = 0; i < g_ref_cache_count; i++) {
		if (strcmp(g_ref_cache[i].name, test_name) == 0) {
			return &g_ref_cache[i];
		}
	}
	return NULL;
}

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

		// Preload all reference images into RAM
		preload_all_references();
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

/**
 * Capture framebuffer to CPU memory using PBO for optimization.
 */
static void capture_frame_pbo(unsigned char* pixels, int fb_width,
                              int fb_height, size_t pixel_data_size)
{
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	if (g_pbo[0] != 0) {
		int current_pbo = g_pbo_index;
		glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[current_pbo]);
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, 0);

		if (g_pbo_sync[current_pbo]) {
			glDeleteSync(g_pbo_sync[current_pbo]);
		}
		g_pbo_sync[current_pbo] =
		    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		glClientWaitSync(g_pbo_sync[current_pbo],
		                 GL_SYNC_FLUSH_COMMANDS_BIT,
		                 DEFAULT_SYNC_TIMEOUT);

		void* mapped = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		if (mapped) {
			(void)memcpy(pixels, mapped, pixel_data_size);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		g_pbo_index = (g_pbo_index + 1) % 2;
	} else {
		glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
		             GL_UNSIGNED_BYTE, pixels);
	}
}

static void verify_reference_image(int width, int height,
                                   unsigned char* current_pixels,
                                   const char* face_name)
{
	size_t pixel_data_size = (size_t)(width * height * BYTES_PER_PIXEL);

	unsigned char* ref_pixels = NULL;
	int ref_w = 0;
	int ref_h = 0;
	bool is_cached = false;

	// Try cache first
	CachedRef* cached = get_cached_reference(face_name);
	if (cached) {
		ref_pixels = cached->data;
		ref_w = cached->width;
		ref_h = cached->height;
		is_cached = true;
	} else {
		// Fallback to disk
		char ref_path[PATH_BUF_SIZE];
		(void)snprintf(ref_path, sizeof(ref_path),
		               "tests/references/ref_%s.png", face_name);
		int ref_channels = 0;
		ref_pixels = stbi_load(ref_path, &ref_w, &ref_h, &ref_channels,
		                       BYTES_PER_PIXEL);
	}

	if (ref_pixels == NULL) {
		char err_msg[ERR_BUF_SIZE];
		(void)snprintf(err_msg, sizeof(err_msg),
		               "Reference image %s not found.", face_name);
		TEST_FAIL_MESSAGE(err_msg);
		return;
	}

	if (ref_w != width || ref_h != height) {
		if (!is_cached) {
			stbi_image_free(ref_pixels);
		}
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

typedef void (*PreRenderFunc)(void*);

static void pipeline_run_test_loop(const char* test_tag,
                                   PreRenderFunc pre_render, void* user_data)
{
	int fb_width = 0;
	int fb_height = 0;
	glfwGetFramebufferSize(g_test_app.window, &fb_width, &fb_height);
	size_t pixel_data_size =
	    (size_t)(fb_width * fb_height * BYTES_PER_PIXEL);

	unsigned char* pixels[2];
	pixels[0] = (unsigned char*)malloc(pixel_data_size);
	pixels[1] = (unsigned char*)malloc(pixel_data_size);

	for (int i = 0; i < NUM_VIEWPOINTS + 1; i++) {
		int current = i % 2;
		int prev = (i + 1) % 2;

		// --- STEP A: Start Render N (if not done) ---
		if (i < NUM_VIEWPOINTS) {
			const ViewPoint* vpoint = &G_VIEWPOINTS[i];
			glm_vec3_copy((vec3){vpoint->pos[0], vpoint->pos[1],
			                     vpoint->pos[2]},
			              g_test_app.camera.position);
			glm_vec3_copy(
			    (vec3){vpoint->world_up[0], vpoint->world_up[1],
			           vpoint->world_up[2]},
			    g_test_app.camera.world_up);
			g_test_app.camera.yaw = vpoint->yaw;
			g_test_app.camera.pitch = vpoint->pitch;
			camera_update_vectors(&g_test_app.camera);

			if (pre_render) {
				pre_render(user_data);
			}

			app_update(&g_test_app);
			renderer_draw_frame(
			    &g_test_app, &g_test_app.scene,
			    &g_test_app.postprocess, &g_test_app.camera,
			    &g_test_app.gpu_profiler, &g_test_app.timeline_ui,
			    &g_test_app.env_mgr, &g_test_app.notifier,
			    &g_test_app.effect_bench, g_test_app.width,
			    g_test_app.height, g_test_app.delta_time,
			    g_test_app.frame_count, g_test_app.log_gpu_metrics);

			// Start async readback for Viewpoint I
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[current]);
			glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
			             GL_UNSIGNED_BYTE, 0);

			if (g_pbo_sync[current]) {
				glDeleteSync(g_pbo_sync[current]);
			}
			g_pbo_sync[current] =
			    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		}

		// --- STEP B: Verify Render N-1 ---
		if (i > 0) {
			int view_idx = i - 1;
			const ViewPoint* prev_vpoint = &G_VIEWPOINTS[view_idx];

			// Wait for PBO Readback of Viewpoint N-1
			glClientWaitSync(g_pbo_sync[prev],
			                 GL_SYNC_FLUSH_COMMANDS_BIT,
			                 DEFAULT_SYNC_TIMEOUT);

			glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[prev]);
			void* mapped =
			    glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			if (mapped) {
				(void)memcpy(pixels[prev], mapped,
				             pixel_data_size);
				glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			}
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

			flip_image_vertically(fb_width, fb_height,
			                      pixels[prev]);

			char test_name[PATH_BUF_SIZE];
			if (test_tag) {
				(void)snprintf(test_name, sizeof(test_name),
				               "%s_subtle_%s",
				               prev_vpoint->name, test_tag);
			} else {
				// Base integration test (multi-view)
				(void)strncpy(test_name, prev_vpoint->name,
				              PATH_BUF_SIZE - 1);
			}

			if (getenv("GEN_REFS") != NULL) {
				char ref_path[PATH_BUF_SIZE];
				(void)snprintf(ref_path, sizeof(ref_path),
				               "tests/references/ref_%s.png", test_name);
				(void)stbi_write_png(
				    ref_path, fb_width, fb_height,
				    BYTES_PER_PIXEL, pixels[prev],
				    fb_width * BYTES_PER_PIXEL);
			} else {
				verify_reference_image(fb_width, fb_height,
				                       pixels[prev], test_name);
			}
		}
	}

	free(pixels[0]);
	free(pixels[1]);
}

static void apply_subtle_none(void* data)
{
	(void)data;
	postprocess_apply_preset(&g_test_app.postprocess, &PRESET_SUBTLE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);
}

static void apply_subtle_bloom(void* data)
{
	(void)data;
	postprocess_apply_preset(&g_test_app.postprocess, &PRESET_SUBTLE);
	postprocess_enable(&g_test_app.postprocess, POSTFX_BLOOM);
	postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);
}

static void apply_subtle_auto_exposure(void* data)
{
	(void)data;
	postprocess_apply_preset(&g_test_app.postprocess, &PRESET_SUBTLE);
	postprocess_enable(&g_test_app.postprocess, POSTFX_AUTO_EXPOSURE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);
	g_test_app.log_gpu_metrics = 1;

	// Note: warmup frames are handled inside the app_update cycles in the
	// main loop. We might need a special case for this if 16 frames is not
	// enough.
}

static void apply_subtle_fxaa(void* data)
{
	(void)data;
	postprocess_apply_preset(&g_test_app.postprocess, &PRESET_SUBTLE);
	postprocess_enable(&g_test_app.postprocess, POSTFX_FXAA);
	postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);
}

static void apply_subtle_dof(void* data)
{
	(void)data;
	postprocess_apply_preset(&g_test_app.postprocess, &PRESET_SUBTLE);
	postprocess_enable(&g_test_app.postprocess, POSTFX_DOF);
	postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);
}

static void apply_subtle_motion_blur(void* data)
{
	(void)data;
	postprocess_apply_preset(&g_test_app.postprocess, &PRESET_SUBTLE);
	postprocess_enable(&g_test_app.postprocess, POSTFX_MOTION_BLUR);
	postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
	postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);
}

/**
 * Integration Test: Full lifecycle and single frame rendering validation
 */
static void test_app_render_multi_view(void)
{
	TEST_ASSERT_TRUE_MESSAGE(g_app_initialized,
	                         "App should be initialized");
	g_test_app.postprocess.active_effects = 0;
	pipeline_run_test_loop(NULL, NULL, NULL);
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

		capture_frame_pbo(pixels, fb_width, fb_height, pixel_data_size);

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
		g_test_app.log_gpu_metrics = 1;

		// Disable Vignette and Grain as requested
		postprocess_disable(&g_test_app.postprocess, POSTFX_VIGNETTE);
		postprocess_disable(&g_test_app.postprocess, POSTFX_GRAIN);

		// Warmup auto-exposure (16 frames instead of 128 for speed)
		const int warmup_frames = 16;
		g_test_app.delta_time = 1.0 / 60.0;
		for (int frame = 0; frame < warmup_frames; frame++) {
			g_test_app.frame_count++;
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

		capture_frame_pbo(pixels, fb_width, fb_height, pixel_data_size);

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

		capture_frame_pbo(pixels, fb_width, fb_height, pixel_data_size);

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

		capture_frame_pbo(pixels, fb_width, fb_height, pixel_data_size);

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

		capture_frame_pbo(pixels, fb_width, fb_height, pixel_data_size);

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

		// Optimization: Use PBO if available
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		if (g_pbo[0] != 0) {
			int current_pbo = g_pbo_index;
			glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[current_pbo]);
			glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
			             GL_UNSIGNED_BYTE, 0);

			if (g_pbo_sync[current_pbo]) {
				glDeleteSync(g_pbo_sync[current_pbo]);
			}
			g_pbo_sync[current_pbo] =
			    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

			glClientWaitSync(g_pbo_sync[current_pbo],
			                 GL_SYNC_FLUSH_COMMANDS_BIT,
			                 DEFAULT_SYNC_TIMEOUT);

			void* mapped =
			    glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
			if (mapped) {
				(void)memcpy(pixels, mapped, pixel_data_size);
				glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			}
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			g_pbo_index = (g_pbo_index + 1) % 2;
		} else {
			glReadPixels(0, 0, fb_width, fb_height, GL_RGB,
			             GL_UNSIGNED_BYTE, pixels);
		}

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
