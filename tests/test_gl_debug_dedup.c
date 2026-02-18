#include "unity.h"
#include "gl_debug.h"
#include "glad/glad.h"
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

/* Mock Log System */
static int g_log_call_count = 0;
static LogLevel g_last_log_level = LOG_LEVEL_NOTSET;
static char g_last_log_tag[64];
static char g_last_log_message[512];

/* Implementation of log.h functions required by gl_debug.c */
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
    g_log_call_count++;
    g_last_log_level = level;
    if (tag) {
        strncpy(g_last_log_tag, tag, sizeof(g_last_log_tag) - 1);
        g_last_log_tag[sizeof(g_last_log_tag) - 1] = '\0';
    }

    va_list args;
    va_start(args, format);
    vsnprintf(g_last_log_message, sizeof(g_last_log_message), format, args);
    va_end(args);
}

void log_set_callback(LogCallback callback) { (void)callback; }
void log_set_level(LogLevel level) { (void)level; }
LogLevel log_get_level(void) { return LOG_LEVEL_DEBUG; }

/* Include the source file directly to access static functions/state */
#include "../src/gl_debug.c"

/* Test Setup */
void setUp(void)
{
    g_log_call_count = 0;
    g_last_log_level = LOG_LEVEL_NOTSET;
    memset(g_last_log_tag, 0, sizeof(g_last_log_tag));
    memset(g_last_log_message, 0, sizeof(g_last_log_message));
}

void tearDown(void)
{
}

/* Test Cases */

void test_gl_debug_callback_deduplication_removed(void)
{
    /*
     * Test Strategy:
     * 1. Call gl_debug_callback with a specific message ID.
     * 2. Verify log is called.
     * 3. Call gl_debug_callback AGAIN with the SAME message ID.
     * 4. Verify log is called AGAIN (count should be 2).
     *
     * If deduplication was active, count would be 1.
     */

    GLuint message_id = 0x1234;
    const char* message = "Test Error Message";

    /* First Call */
    gl_debug_callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, message_id,
                      GL_DEBUG_SEVERITY_HIGH, strlen(message), message, NULL);

    TEST_ASSERT_EQUAL_INT(1, g_log_call_count);
    TEST_ASSERT_EQUAL_INT(LOG_LEVEL_ERROR, g_last_log_level);
    TEST_ASSERT_EQUAL_STRING("id: 0x1234, source: API, type: Error, severity: High, message: Test Error Message", g_last_log_message);

    /* Second Call (Same ID) */
    gl_debug_callback(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, message_id,
                      GL_DEBUG_SEVERITY_HIGH, strlen(message), message, NULL);

    /* With deduplication removed, count should be 2 */
    TEST_ASSERT_EQUAL_INT(2, g_log_call_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_gl_debug_callback_deduplication_removed);
    return UNITY_END();
}
