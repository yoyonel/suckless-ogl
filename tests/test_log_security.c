#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEST_COMPILER
#define TEST_COMPILER "cc"
#endif

#ifndef TEST_INCLUDE_DIR
#define TEST_INCLUDE_DIR "include"
#endif

/*
 * This test verifies that the `log_message` function has the correct
 * `__attribute__((format(printf, ...)))` annotation by attempting to
 * compile a small C program that violates the format string rules.
 *
 * If the attribute is present and working, the compiler (GCC/Clang)
 * should emit a warning/error (when -Wformat is enabled) and the
 * compilation command should fail (when -Werror=format is enabled).
 */

void setUp(void)
{
}

void tearDown(void)
{
	(void)remove("temp_test_log_fmt.c");
	(void)remove("temp_test_log_fmt.o");
}

void test_log_format_validation_fails_compilation(void)
{
	// 1. Create a temporary C file with invalid log usage
	FILE* f_ptr = fopen("temp_test_log_fmt.c", "w");
	TEST_ASSERT_NOT_NULL_MESSAGE(f_ptr, "Failed to create temp test file");

	fprintf(f_ptr, "#include \"log.h\"\n");
	fprintf(f_ptr, "#include <stdio.h>\n");
	fprintf(f_ptr, "int main(void) {\n");
	// Deliberate error: %d expects int, but string is passed
	fprintf(f_ptr,
	        "    log_message(LOG_LEVEL_INFO, \"TEST\", \"Value: %%d\", "
	        "\"string\");\n");
	fprintf(f_ptr, "    return 0;\n");
	fprintf(f_ptr, "}\n");
	fclose(f_ptr);

	// 2. Construct compilation command dynamically
	char cmd[2048];
	snprintf(cmd, sizeof(cmd),
	         "%s -c temp_test_log_fmt.c -I%s -Wall -Wformat "
	         "-Werror=format -o temp_test_log_fmt.o > /dev/null 2>&1",
	         TEST_COMPILER, TEST_INCLUDE_DIR);

	// 3. Execute command
	int status = system(cmd);

	// 4. Assert Failure
	// system() returns exit code. We EXPECT it to be non-zero (failure).
	if (status == 0) {
		TEST_FAIL_MESSAGE(
		    "Compilation succeeded, but should have failed! "
		    "Format string validation is likely missing.");
	} else {
		// Success: The compiler rejected the bad code.
		// (Implicitly passing the test)
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_log_format_validation_fails_compilation);
	return UNITY_END();
}
