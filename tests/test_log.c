#include "log.h"
#include "unity.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CAPTURE_FILE "test_log_capture.txt"

void setUp(void)
{
	// Clean slate for capture file
	remove(CAPTURE_FILE);
}

void tearDown(void)
{
	remove(CAPTURE_FILE);
}

// Helper to read the captured file content
static void assert_capture_contains(const char* expected_level,
                                    const char* expected_tag,
                                    const char* expected_msg)
{
	FILE* f = fopen(CAPTURE_FILE, "r");
	TEST_ASSERT_NOT_NULL_MESSAGE(f, "Failed to open capture file");

	char buffer[1024];
	if (fgets(buffer, sizeof(buffer), f) == NULL) {
		TEST_FAIL_MESSAGE("Capture file is empty");
	}
	fclose(f);

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
	fflush(stderr);
	fflush(stdout);
	stderr_backup = dup(STDERR_FILENO);
	stdout_backup = dup(STDOUT_FILENO);

	int fd = open(CAPTURE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		perror("Failed to open capture file");
		return;
	}

	// Redirect both to the same file
	dup2(fd, STDERR_FILENO);
	dup2(fd, STDOUT_FILENO);
	close(fd);
}

void restore_streams(void)
{
	fflush(stderr);
	fflush(stdout);

	dup2(stderr_backup, STDERR_FILENO);
	close(stderr_backup);
	stderr_backup = -1;

	dup2(stdout_backup, STDOUT_FILENO);
	close(stdout_backup);
	stdout_backup = -1;
}

void test_log_info(void)
{
	redirect_streams();
	LOG_INFO("TEST_TAG", "Simple info message");
	restore_streams();

	assert_capture_contains("INFO", "TEST_TAG", "Simple info message");
}

void test_log_warn(void)
{
	redirect_streams();
	LOG_WARN("TEST_TAG", "Warning content");
	restore_streams();

	assert_capture_contains("WARN", "TEST_TAG", "Warning content");
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
	redirect_streams();
	LOG_DEBUG("DBG", "Debug info");
	restore_streams();

	assert_capture_contains("DEBUG", "DBG", "Debug info");
}

void test_log_formatting(void)
{
	redirect_streams();
	LOG_INFO("FMT", "Value is %d", 42);
	restore_streams();

	assert_capture_contains("INFO", "FMT", "Value is 42");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_log_info);
	RUN_TEST(test_log_warn);
	RUN_TEST(test_log_error);
	RUN_TEST(test_log_debug);
	RUN_TEST(test_log_formatting);
	return UNITY_END();
}
