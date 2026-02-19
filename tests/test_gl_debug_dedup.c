#include "mock_gl_standalone.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

/* Mock LOG macros to capture output */
static int g_log_count = 0;
static char g_last_log_message[256];

/* Define LOG_H to prevent inclusion of the real log.h */
#define LOG_H
/* We need to include enum LogLevel if log.h defines it, but gl_debug.c doesn't
   use it directly, only passed to log macros. */

/* Redefine macros used in gl_debug.c */
#define LOG_ERROR(tag, fmt, ...)                                              \
	do {                                                                  \
		snprintf(g_last_log_message, sizeof(g_last_log_message), fmt, \
		         ##__VA_ARGS__);                                      \
		g_log_count++;                                                \
	} while (0)
#define LOG_WARNING(tag, fmt, ...)                                            \
	do {                                                                  \
		snprintf(g_last_log_message, sizeof(g_last_log_message), fmt, \
		         ##__VA_ARGS__);                                      \
		g_log_count++;                                                \
	} while (0)
#define LOG_INFO(tag, fmt, ...)                                               \
	do {                                                                  \
		snprintf(g_last_log_message, sizeof(g_last_log_message), fmt, \
		         ##__VA_ARGS__);                                      \
		g_log_count++;                                                \
	} while (0)

/* Include source under test */
#include "gl_debug.c"

void setUp(void)
{
	mock_gl_reset_calls();
	g_log_count = 0;
	memset(g_last_log_message, 0, sizeof(g_last_log_message));
}

void tearDown(void)
{
}

void test_gl_debug_dedup_behavior(void)
{
	/* Initialize debug */
	setup_opengl_debug();
	/* Reset log count to ignore initialization message */
	g_log_count = 0;

	/* Trigger error once */
	mock_gl_trigger_debug_callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR,
	                               1001, GL_DEBUG_SEVERITY_HIGH, "Error 1");
	TEST_ASSERT_EQUAL(1, g_log_count);

	/* Trigger SAME error again */
	mock_gl_trigger_debug_callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR,
	                               1001, GL_DEBUG_SEVERITY_HIGH, "Error 1");

	/* If deduplication is active (current behavior), count should still
	 * be 1. */
	/* We want to enforce NO deduplication for High Sensitivity monitoring.
	 */
	/* So we assert 2. If it stays 1, the test fails, indicating
	 * deduplication is still active. */
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    2, g_log_count,
	    "Debug callback suppressed duplicate message! Integrity monitor "
	    "requires all logs.");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_gl_debug_dedup_behavior);
	return UNITY_END();
}
