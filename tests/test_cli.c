#include "cli.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

static const size_t CLI_LONG_ARG_SIZE = 1024 * 1024;
static const char FILL_CHAR = 'A';

void setUp(void)
{
}

void tearDown(void)
{
}

void test_cli_no_args(void)
{
	char* argv[] = {"app"};
	TEST_ASSERT_EQUAL(CLI_ACTION_CONTINUE, cli_handle_args(1, argv).action);
}

void test_cli_help_short(void)
{
	char* argv[] = {"app", "-h"};
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_SUCCESS, cli_handle_args(2, argv).action);
}

void test_cli_help_long(void)
{
	char* argv[] = {"app", "--help"};
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_SUCCESS, cli_handle_args(2, argv).action);
}

void test_cli_unknown_arg(void)
{
	char* argv[] = {"app", "--unknown"};
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_FAILURE, cli_handle_args(2, argv).action);
}

void test_cli_partial_match(void)
{
	char* argv[] = {"app", "--h"};
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_FAILURE, cli_handle_args(2, argv).action);
}

void test_cli_api_arg(void)
{
	char* argv_gl[] = {"app", "--api", "opengl"};
	CliResult result1 = cli_handle_args(3, argv_gl);
	TEST_ASSERT_EQUAL(CLI_ACTION_CONTINUE, result1.action);
	TEST_ASSERT_EQUAL(API_OPENGL, result1.api);

	char* argv_vk[] = {"app", "--api", "vulkan"};
	CliResult result2 = cli_handle_args(3, argv_vk);
	TEST_ASSERT_EQUAL(CLI_ACTION_CONTINUE, result2.action);
	TEST_ASSERT_EQUAL(API_VULKAN, result2.api);

	char* argv_unknown_api[] = {"app", "--api", "dx12"};
	CliResult result3 = cli_handle_args(3, argv_unknown_api);
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_FAILURE, result3.action);

	char* argv_missing_api[] = {"app", "--api"};
	CliResult result4 = cli_handle_args(2, argv_missing_api);
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_FAILURE, result4.action);
}

void test_cli_very_long_arg(void)
{
	/* Create a 1MB argument */
	size_t size = CLI_LONG_ARG_SIZE;
	char* long_arg = (char*)malloc(size + 1);
	TEST_ASSERT_NOT_NULL(long_arg);
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.bzero)
	(void)memset(long_arg, FILL_CHAR, size);
	long_arg[size] = '\0';

	char* argv[] = {"app", long_arg};
	/* Should fail gracefully (exit failure) without crashing */
	TEST_ASSERT_EQUAL(CLI_ACTION_EXIT_FAILURE, cli_handle_args(2, argv).action);
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
	RUN_TEST(test_cli_api_arg);
	RUN_TEST(test_cli_very_long_arg);
	return UNITY_END();
}
