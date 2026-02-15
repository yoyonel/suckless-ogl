#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include headers from the project */
#include "glad/glad.h"
#include "log.h"
#include "mock_gl_standalone.h"
#include "unity.h"

/* Mock Logging Implementation */
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
	/* In a real test, we might capture this output */
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

/* Include implementation to test internal static functions */
#include "shader.c"

/* Test Cases */

void setUp(void)
{
	mock_gl_reset_calls();
}

void tearDown(void)
{
}

void test_get_dir_from_path_truncation(void)
{
	char out_buf[10];
	const char* long_path = "/home/user/very/long/path/to/file.glsl";
	// Directory is "/home/user/very/long/path/to/" -> len 29
	// Buffer size is 10
	// Expected: Failure (return false)

	bool result = get_dir_from_path(long_path, out_buf, sizeof(out_buf));
	TEST_ASSERT_FALSE_MESSAGE(
	    result, "get_dir_from_path should fail on truncation");
}

void test_get_dir_from_path_success(void)
{
	char out_buf[30];
	const char* path = "/tmp/file.glsl";
	// Directory is "/tmp/" -> len 5
	// Buffer size is 30
	// Expected: Success (return true), buffer contains "/tmp/"

	bool result = get_dir_from_path(path, out_buf, sizeof(out_buf));
	TEST_ASSERT_TRUE_MESSAGE(result, "get_dir_from_path should succeed");
	TEST_ASSERT_EQUAL_STRING_MESSAGE("/tmp/", out_buf,
	                                 "get_dir_from_path output correct");
}

void test_parse_include_path_truncation(void)
{
	char out_buf[5];
	const char* input = "\"header.glsl\"";  // Length 11 ("header.glsl")
	// Buffer size 5
	// Expected: Failure (return NULL)

	const char* result =
	    parse_include_path(input, out_buf, sizeof(out_buf));
	TEST_ASSERT_NULL_MESSAGE(
	    result, "parse_include_path should fail on truncation");
}

void test_parse_include_path_success(void)
{
	char out_buf[20];
	const char* input = "\"header.glsl\"";  // Length 11
	// Buffer size 20
	// Expected: Success (return non-NULL), buffer contains "header.glsl"

	const char* result =
	    parse_include_path(input, out_buf, sizeof(out_buf));
	TEST_ASSERT_NOT_NULL_MESSAGE(result,
	                             "parse_include_path should succeed");
	TEST_ASSERT_EQUAL_STRING_MESSAGE("header.glsl", out_buf,
	                                 "parse_include_path output correct");
}

void test_is_safe_path_cases(void)
{
	/* 1. Valid Paths */
	TEST_ASSERT_TRUE_MESSAGE(is_safe_path("shader.glsl"),
	                         "Simple filename should be safe");
	TEST_ASSERT_TRUE_MESSAGE(is_safe_path("dir/shader.glsl"),
	                         "Relative path in subdir should be safe");
	TEST_ASSERT_TRUE_MESSAGE(is_safe_path("./shader.glsl"),
	                         "Explicit current dir should be safe");

	/* 2. Parent Directory Traversal */
	TEST_ASSERT_FALSE_MESSAGE(
	    is_safe_path("../shader.glsl"),
	    "Parent directory traversal (start) should be unsafe");
	TEST_ASSERT_FALSE_MESSAGE(
	    is_safe_path("dir/../shader.glsl"),
	    "Parent directory traversal (middle) should be unsafe");
	TEST_ASSERT_FALSE_MESSAGE(is_safe_path(".."),
	                          "Just parent dir should be unsafe");

	/* 3. Absolute Paths */
	TEST_ASSERT_FALSE_MESSAGE(is_safe_path("/etc/passwd"),
	                          "Absolute path (start) should be unsafe");
	TEST_ASSERT_FALSE_MESSAGE(is_safe_path("/shader.glsl"),
	                          "Absolute path (root) should be unsafe");

	/* 4. Windows Separators (Backslashes) */
	TEST_ASSERT_FALSE_MESSAGE(
	    is_safe_path("..\\shader.glsl"),
	    "Windows backslash traversal should be unsafe");
	TEST_ASSERT_FALSE_MESSAGE(
	    is_safe_path("dir\\shader.glsl"),
	    "Windows backslash separator should be unsafe (linux-only app)");

	/* 5. URL Schemes / Protocol handlers */
	TEST_ASSERT_FALSE_MESSAGE(
	    is_safe_path("http://example.com/shader.glsl"),
	    "URL with protocol should be unsafe");
	TEST_ASSERT_FALSE_MESSAGE(is_safe_path("file:///shader.glsl"),
	                          "File protocol should be unsafe");
}

void test_shader_read_file_blocks_unsafe_paths(void)
{
	// Attempt to read a file using parent directory traversal
	// This file likely doesn't exist at this path relative to the test
	// binary, but the security check should happen BEFORE fopen.

	const char* unsafe_path = "../Makefile";
	char* content = shader_read_file(unsafe_path);

	TEST_ASSERT_NULL_MESSAGE(
	    content, "shader_read_file should return NULL for unsafe paths");

	// We can't easily check if it returned NULL due to fopen fail or
	// security check without mocking fopen or capturing logs. But given
	// is_safe_path("../Makefile") is false, it MUST fail securely.
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_get_dir_from_path_truncation);
	RUN_TEST(test_get_dir_from_path_success);
	RUN_TEST(test_parse_include_path_truncation);
	RUN_TEST(test_parse_include_path_success);
	RUN_TEST(test_is_safe_path_cases);
	RUN_TEST(test_shader_read_file_blocks_unsafe_paths);
	return UNITY_END();
}
