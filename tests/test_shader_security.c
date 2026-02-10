// tests/test_shader_security.c
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
static const size_t KB_SIZE = 1024;
static const size_t MB_SIZE_16 = 16 * 1024 * 1024;
static const size_t SIZE_INCREMENT = 1;
static const int SEEK_SUCCESS = 0;
static const char TEST_BYTE = 'A';

static void create_large_file(const char* filename, size_t size)
{
	FILE* file = fopen(filename, "wb");
	if (!file) {
		TEST_FAIL_MESSAGE("Failed to create test file");
	}

	/* Write one byte at the end to set size */
	if (fseek(file, (long)(size - SIZE_INCREMENT), SEEK_SET) !=
	    SEEK_SUCCESS) {
		(void)fclose(file);
		TEST_FAIL_MESSAGE("Failed to seek in test file");
	}
	if (fputc(TEST_BYTE, file) == EOF) {
		(void)fclose(file);
		TEST_FAIL_MESSAGE("Failed to write to test file");
	}
	(void)fclose(file);
}

static void create_file_with_content(const char* filename, const char* content)
{
	FILE* file = fopen(filename, "w");
	if (!file) {
		TEST_FAIL_MESSAGE("Failed to create test file");
	}
	if (fputs(content, file) == EOF) {
		(void)fclose(file);
		TEST_FAIL_MESSAGE("Failed to write to test file");
	}
	(void)fclose(file);
}

void test_shader_read_too_large(void)
{
	const char* filename = "too_large.glsl";
	create_large_file(filename, MB_SIZE_16 + SIZE_INCREMENT);

	char* result = shader_read_file(filename);
	/* Expect NULL because we want to enforce the limit */
	TEST_ASSERT_NULL_MESSAGE(
	    result, "Should fail for files larger than MAX_SHADER_SOURCE_SIZE");

	(void)remove(filename);
	if (result)
		free(result);
}

void test_shader_read_limit(void)
{
	const char* filename = "limit.glsl";
	create_large_file(filename, MB_SIZE_16);

	char* result = shader_read_file(filename);
	/* Expect non-NULL because it's within limit */
	TEST_ASSERT_NOT_NULL_MESSAGE(
	    result, "Should succeed for files equal to MAX_SHADER_SOURCE_SIZE");

	(void)remove(filename);
	free(result);
}

void test_recursion_limit(void)
{
	/* MAX_INCLUDE_DEPTH is 16.
	   Depth 0 (root)
	   Depth 1 (include 1)
	   ...
	   Depth 17 (include 17) -> Should Fail
	*/
	const int DEPTH_LIMIT = 16;
	const int TEST_DEPTH = DEPTH_LIMIT + 2; /* 18 files total */
	char filenames[TEST_DEPTH][32];

	for (int i = 0; i < TEST_DEPTH; ++i) {
		(void)sprintf(filenames[i], "rec_%d.glsl", i);
	}

	/* Create chain */
	/* rec_0 includes rec_1 ... rec_16 includes rec_17 */
	for (int i = 0; i < TEST_DEPTH - 1; ++i) {
		char content[64];
		(void)sprintf(content, "@header \"%s\"\n", filenames[i + 1]);
		create_file_with_content(filenames[i], content);
	}
	/* Last file is empty or simple content */
	create_file_with_content(filenames[TEST_DEPTH - 1], "// leaf\n");

	/* Try to read root */
	char* result = shader_read_file(filenames[0]);

	/* Should fail because of recursion limit */
	TEST_ASSERT_NULL_MESSAGE(
	    result, "Should fail when recursion depth exceeds limit");

	if (result)
		free(result);

	/* Cleanup */
	for (int i = 0; i < TEST_DEPTH; ++i) {
		(void)remove(filenames[i]);
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_shader_read_too_large);
	RUN_TEST(test_shader_read_limit);
	RUN_TEST(test_recursion_limit);
	return UNITY_END();
}
