#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock Dependencies
#include "glad/glad.h"
#include "log.h"
#include "mock_gl_standalone.h"
#include "mock_stb_image_standalone.h"
#include "stb_image.h"
#include "unity.h"

// Define mocks for log functions
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

// Include source under test
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
	mock_gl_reset_calls();
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

	// Enable TOCTOU simulation: info returns 10x10, load returns 9000x9000
	mock_stbi_set_toctou_simulation(1);

	// This should return NULL because dimensions change between info and
	// load
	float* data = texture_load_pixels(TEST_FILENAME, &w, &h, &c);

	if (data != NULL) {
		stbi_image_free(data);
	}

	TEST_ASSERT_NULL_MESSAGE(
	    data,
	    "texture_load_pixels should return NULL when dimensions exceed MAX "
	    "after load");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_texture_load_pixels_detects_toctou_dimension_mismatch);
	return UNITY_END();
}
