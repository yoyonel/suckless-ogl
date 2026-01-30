#include "cli.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

#define TEST_CLI_LONG_ARG_SIZE (1024 * 1024)

void setUp(void)
{
}

void tearDown(void)
{
}

void test_cli_no_args(void)
{
	char* argv[] = {"app"};
	TEST_ASSERT_EQUAL(CLI_ACTION_CONTINUE, cli_handle_args(1, argv));
}

void test_cli_help_short(void)
{
	char* argv[] = {"app", "-h"};
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_SUCCESS, cli_handle_args(2, argv));
}

void test_cli_help_long(void)
{
	char* argv[] = {"app", "--help"};
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_SUCCESS, cli_handle_args(2, argv));
}

void test_cli_unknown_arg(void)
{
	char* argv[] = {"app", "--unknown"};
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_FAILURE, cli_handle_args(2, argv));
}

void test_cli_partial_match(void)
{
	char* argv[] = {"app", "--h"};
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_FAILURE, cli_handle_args(2, argv));
}

void test_cli_very_long_arg(void)
{
	/* Create a 1MB argument */
	size_t size = TEST_CLI_LONG_ARG_SIZE;
	char* long_arg = malloc(size + 1);
	TEST_ASSERT_NOT_NULL(long_arg);
	(void)memset(long_arg, 'A', size);
	long_arg[size] = '\0';

	char* argv[] = {"app", long_arg};
	/* Should fail gracefully (exit failure) without crashing */
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_FAILURE, cli_handle_args(2, argv));
	free(long_arg);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_cli_no_args);
	RUN_TEST(test_cli_help_short);
	RUN_TEST(test_cli_help_long);
	RUN_TEST(test_cli_unknown_arg);
	RUN_TEST(test_cli_partial_match);
	RUN_TEST(test_cli_very_long_arg);
	return UNITY_END();
}
