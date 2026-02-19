#include "unity.h"
#include "utils.h"
#include <stdbool.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_is_safe_filename_valid(void)
{
	TEST_ASSERT_TRUE(is_safe_filename("sky.hdr"));
	TEST_ASSERT_TRUE(is_safe_filename("env_map_2.hdr"));
	TEST_ASSERT_TRUE(is_safe_filename("test.data"));
	TEST_ASSERT_TRUE(is_safe_filename("A"));
}

void test_is_safe_filename_traversal(void)
{
	TEST_ASSERT_FALSE(is_safe_filename("../etc/passwd"));
	TEST_ASSERT_FALSE(is_safe_filename("subdir/../file"));
	TEST_ASSERT_FALSE(is_safe_filename(".."));
	TEST_ASSERT_FALSE(is_safe_filename("..\\win.ini"));
}

void test_is_safe_filename_current_dir(void)
{
	TEST_ASSERT_FALSE(is_safe_filename("."));
}

void test_is_safe_filename_separators(void)
{
	TEST_ASSERT_FALSE(is_safe_filename("subdir/file"));
	TEST_ASSERT_FALSE(is_safe_filename("C:\\file"));
	TEST_ASSERT_FALSE(is_safe_filename("/absolute/path"));
}

void test_is_safe_filename_null_and_empty(void)
{
	TEST_ASSERT_FALSE(is_safe_filename(NULL));
	TEST_ASSERT_TRUE(is_safe_filename(
	    ""));  // Empty string is technically safe from traversal, but maybe
	           // should be rejected? The current implementation allows it.
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_is_safe_filename_valid);
	RUN_TEST(test_is_safe_filename_traversal);
	RUN_TEST(test_is_safe_filename_current_dir);
	RUN_TEST(test_is_safe_filename_separators);
	RUN_TEST(test_is_safe_filename_null_and_empty);
	return UNITY_END();
}
