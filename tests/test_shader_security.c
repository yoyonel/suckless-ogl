#include "shader.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

/* MAX_SHADER_SOURCE_SIZE is defined as 16 * 1024 * 1024 in src/shader.c */
#define MAX_SIZE (16 * 1024 * 1024)

static void create_large_file(const char* filename, size_t size)
{
	FILE* f = fopen(filename, "wb");
	if (!f) {
		TEST_FAIL_MESSAGE("Failed to create test file");
	}

	/* Write one byte at the end to set size */
	if (fseek(f, (long)(size - 1), SEEK_SET) != 0) {
		fclose(f);
		TEST_FAIL_MESSAGE("Failed to seek in test file");
	}
	if (fputc('A', f) == EOF) {
		fclose(f);
		TEST_FAIL_MESSAGE("Failed to write to test file");
	}
	fclose(f);
}

void test_shader_read_too_large(void)
{
	const char* filename = "too_large.glsl";
	create_large_file(filename, MAX_SIZE + 1);

	char* result = shader_read_file(filename);
	/* Expect NULL because we want to enforce the limit */
	TEST_ASSERT_NULL_MESSAGE(
	    result, "Should fail for files larger than MAX_SHADER_SOURCE_SIZE");

	remove(filename);
	if (result)
		free(result);
}

void test_shader_read_limit(void)
{
	const char* filename = "limit.glsl";
	create_large_file(filename, MAX_SIZE);

	char* result = shader_read_file(filename);
	/* Expect non-NULL because it's within limit */
	TEST_ASSERT_NOT_NULL_MESSAGE(
	    result, "Should succeed for files equal to MAX_SHADER_SOURCE_SIZE");

	remove(filename);
	free(result);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_shader_read_too_large);
	RUN_TEST(test_shader_read_limit);
	return UNITY_END();
}
