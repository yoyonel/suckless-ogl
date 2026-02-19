#include "glad/glad.h"
#include "log.h"
#include "mock_gl_standalone.h"
#include "unity.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mock Logging */
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
}
void log_set_callback(LogCallback callback)
{
	(void)callback;
}
void log_set_level(LogLevel level)
{
	(void)level;
}
LogLevel log_get_level(void)
{
	return LOG_LEVEL_INFO;
}

/* Include source to test static functions */
#include "shader.c"

void setUp(void)
{
	mock_gl_reset_calls();
}
void tearDown(void)
{
}

void test_ctx_add_chunk_limit(void)
{
	IncludeContext ctx = {0};

	// 1. Fill up to limit - 1
	ctx.total_size = MAX_SHADER_SOURCE_SIZE - 1;

	// 2. Add 1 byte -> Should Succeed
	bool res = ctx_add_chunk(&ctx, "a", 1);
	TEST_ASSERT_TRUE_MESSAGE(
	    res, "Adding small chunk within limit should succeed");
	TEST_ASSERT_EQUAL_UINT_MESSAGE(MAX_SHADER_SOURCE_SIZE, ctx.total_size,
	                               "Total size should reach limit");

	// 3. Add 1 byte -> Should Fail (Exceeds limit)
	res = ctx_add_chunk(&ctx, "b", 1);
	TEST_ASSERT_FALSE_MESSAGE(res,
	                          "Adding chunk exceeding limit should fail");

	// Cleanup to prevent memory leaks (ctx_add_chunk allocates chunk)
	// Since we manually constructed ctx, we need to manually clean up list
	ctx_free(&ctx);
}

void test_ctx_add_chunk_large_overflow(void)
{
	IncludeContext ctx = {0};

	// Add chunk larger than limit
	bool res = ctx_add_chunk(&ctx, "large", MAX_SHADER_SOURCE_SIZE + 1);
	TEST_ASSERT_FALSE_MESSAGE(res, "Adding huge chunk should fail");
	TEST_ASSERT_EQUAL_UINT_MESSAGE(0, ctx.total_size,
	                               "Total size should remain 0");

	ctx_free(&ctx);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_ctx_add_chunk_limit);
	RUN_TEST(test_ctx_add_chunk_large_overflow);
	return UNITY_END();
}
