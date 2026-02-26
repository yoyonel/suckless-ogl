// tests/test_log.c
#include "log.h"
#include "unity.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CAPTURE_FILE "test_log_capture.txt"

static const int BUFFER_SIZE = 1024;
static const mode_t FILE_PERMS = 0644;
static const int TEST_INT_VAL = 42;

void setUp(void)
{
	// Clean slate for capture file
	(void)remove(CAPTURE_FILE);
}

void tearDown(void)
{
	(void)remove(CAPTURE_FILE);
}

// Helper to read the captured file content
static void assert_capture_contains(const char* expected_level,
                                    const char* expected_tag,
                                    const char* expected_msg)
{
	FILE* f_ptr = fopen(CAPTURE_FILE, "r");
	TEST_ASSERT_NOT_NULL_MESSAGE(f_ptr, "Failed to open capture file");

	char buffer[BUFFER_SIZE];
	if (fgets(buffer, (int)sizeof(buffer), f_ptr) == NULL) {
		(void)fclose(f_ptr);
		TEST_FAIL_MESSAGE("Capture file is empty");
	}
	(void)fclose(f_ptr);

	// Check components
	TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, expected_level),
	                             "Level not found in log");
	TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, expected_tag),
	                             "Tag not found in log");
	TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buffer, expected_msg),
	                             "Message not found in log");
}

// Global backup for stderr/stdout
int stderr_backup = -1;
int stdout_backup = -1;

void redirect_streams(void)
{
	(void)fflush(stderr);
	(void)fflush(stdout);
	stderr_backup = dup(STDERR_FILENO);
	stdout_backup = dup(STDOUT_FILENO);

	int fd_tmp =
	    open(CAPTURE_FILE, O_WRONLY | O_CREAT | O_TRUNC, FILE_PERMS);
	if (fd_tmp < 0) {
		perror("Failed to open capture file");
		return;
	}

	// Redirect both to the same file
	(void)dup2(fd_tmp, STDERR_FILENO);
	(void)dup2(fd_tmp, STDOUT_FILENO);
	(void)close(fd_tmp);
}

void restore_streams(void)
{
	(void)fflush(stderr);
	(void)fflush(stdout);

	(void)dup2(stderr_backup, STDERR_FILENO);
	(void)close(stderr_backup);
	stderr_backup = -1;

	(void)dup2(stdout_backup, STDOUT_FILENO);
	(void)close(stdout_backup);
	stdout_backup = -1;
}

void test_log_info(void)
{
	redirect_streams();
	LOG_INFO("TEST_TAG", "Simple info message");
	restore_streams();

	assert_capture_contains("INFO", "TEST_TAG", "Simple info message");
}

void test_log_warning(void)
{
	redirect_streams();
	LOG_WARNING("TEST_TAG", "Warning content");
	restore_streams();

	assert_capture_contains("WARNING", "TEST_TAG", "Warning content");
}

void test_log_error(void)
{
	redirect_streams();
	LOG_ERROR("TEST_TAG", "Error occurred");
	restore_streams();

	assert_capture_contains("ERROR", "TEST_TAG", "Error occurred");
}

void test_log_debug(void)
{
	log_set_level(LOG_LEVEL_DEBUG);
	redirect_streams();
	LOG_DEBUG("DBG", "Debug info");
	restore_streams();
	log_set_level(LOG_LEVEL_INFO);

	assert_capture_contains("DEBUG", "DBG", "Debug info");
}

void test_log_critical(void)
{
	redirect_streams();
	LOG_CRITICAL("TEST_TAG", "Critical failure");
	restore_streams();

	assert_capture_contains("CRITICAL", "TEST_TAG", "Critical failure");
}

void test_log_filtering(void)
{
	log_set_level(LOG_LEVEL_ERROR);

	redirect_streams();
	LOG_INFO("FILTER", "Should not appear");
	LOG_WARNING("FILTER", "Should not appear");
	LOG_ERROR("FILTER", "Should appear");
	restore_streams();

	FILE* f_ptr = fopen(CAPTURE_FILE, "r");
	TEST_ASSERT_NOT_NULL(f_ptr);
	char buffer[BUFFER_SIZE];
	bool found_error = false;
	while (fgets(buffer, (int)sizeof(buffer), f_ptr)) {
		if (strstr(buffer, "INFO") || strstr(buffer, "WARNING")) {
			(void)fclose(f_ptr);
			TEST_FAIL_MESSAGE("Filtered message appeared in log");
		}
		if (strstr(buffer, "ERROR")) {
			found_error = true;
		}
	}
	(void)fclose(f_ptr);
	TEST_ASSERT_TRUE_MESSAGE(found_error,
	                         "Expected ERROR message not found");

	// Reset level for other tests
	log_set_level(LOG_LEVEL_INFO);
}

void test_log_env_var(void)
{
	(void)setenv("OGL_LOG_LEVEL", "DEBUG", 1);
	log_set_level(LOG_LEVEL_DEBUG);
	TEST_ASSERT_EQUAL(LOG_LEVEL_DEBUG, log_get_level());

	log_set_level(LOG_LEVEL_INFO);
}

void test_log_formatting(void)
{
	redirect_streams();
	LOG_INFO("FMT", "Value is %d", TEST_INT_VAL);
	restore_streams();

	assert_capture_contains("INFO", "FMT", "Value is 42");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_log_info);
	RUN_TEST(test_log_warning);
	RUN_TEST(test_log_error);
	RUN_TEST(test_log_debug);
	RUN_TEST(test_log_critical);
	RUN_TEST(test_log_filtering);
	RUN_TEST(test_log_env_var);
	RUN_TEST(test_log_formatting);
	return UNITY_END();
}
