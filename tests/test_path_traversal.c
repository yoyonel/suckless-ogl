#include "shader.h"
#include "unity.h"
#include <stdlib.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_shader_read_file_unsafe(void)
{
	/*
	 * Try to read a known file using an absolute path.
	 * This should be blocked by is_safe_path() which is now called
	 * inside shader_read_file (via load_file_into_ram).
	 */
#ifdef _WIN32
	char* content =
	    shader_read_file("C:\\Windows\\System32\\drivers\\etc\\hosts");
#else
	char* content = shader_read_file("/etc/hosts");
#endif

	if (content) {
		free(content);
		TEST_FAIL_MESSAGE("shader_read_file allowed unsafe path");
	}
}

void test_shader_read_file_unsafe_traversal(void)
{
	/* Try traversal */
	char* content = shader_read_file("../README.md");
	if (content) {
		free(content);
		TEST_FAIL_MESSAGE("shader_read_file allowed traversal path");
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_shader_read_file_unsafe);
	RUN_TEST(test_shader_read_file_unsafe_traversal);
	return UNITY_END();
}
