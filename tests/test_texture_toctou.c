#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock Dependencies
#include "glad/glad.h"
#include "gpu_profiler.h"
#include "log.h"
#include "mocks/standalone/mock_gl_standalone.h"
#include "mocks/standalone/mock_stb_image_standalone.h"
#include "stb_image.h"
#include "unity.h"

/*
 * Global Mocks for functions NOT provided by other files in the target.
 * We don't use 'static' so that other files in the same target (like io.c)
 * can find them, but we don't use macros to avoid poisoning other targets
 * in a Unity build.
 */

// Log Mocks
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
}

void log_set_callback(LogCallback callback)
{
	(void)callback;
}

void log_set_level(LogLevel level)
{
	(void)level;
}

LogLevel log_get_level(void)
{
	return LOG_LEVEL_INFO;
}

// GPU Profiler Mocks
void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color)
{
	(void)profiler;
	(void)name;
	(void)color;
}

void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	(void)profiler;
}

// Include source under test
// Note: We include it here so it can see internal static functions, but we
// don't use macros to override names anymore because we provide global symbols.
#include "../src/texture.c"

static const char* const TEST_FILENAME = "dummy_toctou.hdr";

void setUp(void)
{
	// Create a dummy file
	FILE* f = fopen(TEST_FILENAME, "w");
	if (f) {
		fprintf(f, "DUMMY");
		fclose(f);
	}
	mock_stbi_set_toctou_simulation(0);
	mock_stbi_set_info_dimensions(10, 10, 4);
}

void tearDown(void)
{
	remove(TEST_FILENAME);
}

void test_texture_load_pixels_detects_toctou_dimension_mismatch(void)
{
	int w, h, c;
	mock_stbi_set_toctou_simulation(1);
	float* data = texture_load_pixels(TEST_FILENAME, &w, &h, &c);
	if (data != NULL) {
		stbi_image_free(data);
	}
	TEST_ASSERT_NULL_MESSAGE(data,
	                         "texture_load_pixels should return NULL when "
	                         "dimensions exceed MAX after load");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_load_pixels_detects_toctou_dimension_mismatch);
	return UNITY_END();
}
