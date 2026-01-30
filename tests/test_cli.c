#include "cli.h"
#include "unity.h"
#include <stdlib.h>

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

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_cli_no_args);
	RUN_TEST(test_cli_help_short);
	RUN_TEST(test_cli_help_long);
	RUN_TEST(test_cli_unknown_arg);
	RUN_TEST(test_cli_partial_match);
	return UNITY_END();
}
