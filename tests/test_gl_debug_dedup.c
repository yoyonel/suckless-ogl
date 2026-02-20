#include "log.h"
#include "mock_gl_standalone.h"
#include "unity.h"
#include <stdarg.h>

/* Mock log_message to count calls */
static int g_log_count = 0;
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
	g_log_count++;
}

/* Include source under test directly to access static function and mock log_log
 */
/* We need to define LOG_TAG before including if not careful, but gl_debug.c
 * defines it. */
#include "../src/gl_debug.c"

void setUp(void)
{
	mock_gl_reset_calls();
	g_log_count = 0;
}

void tearDown(void)
{
}

void test_debug_dedup_disabled(void)
{
	// Simulate an error
	gl_debug_callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, 123,
	                  GL_DEBUG_SEVERITY_HIGH, 0, "Error 1", NULL);
	TEST_ASSERT_EQUAL(1, g_log_count);

	// Simulate SAME error again
	// Since we disabled deduplication, this SHOULD verify that it logs
	// again.
	gl_debug_callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, 123,
	                  GL_DEBUG_SEVERITY_HIGH, 0, "Error 1", NULL);

	// Should be 2 now (dedup disabled)
	TEST_ASSERT_EQUAL(2, g_log_count);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_debug_dedup_disabled);
	return UNITY_END();
}
