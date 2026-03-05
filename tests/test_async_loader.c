#define _POSIX_C_SOURCE 199309L
#ifndef GL_COMMON_NO_GLAD
#define GL_COMMON_NO_GLAD
#endif
#ifndef GL_COMMON_NO_GLFW
#define GL_COMMON_NO_GLFW
#endif

#include "async_loader.h"
#include "gl_common.h"
#include "log.h"
#include "platform/platform_time.h"
#include "profiler.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unity.h>

/* --- Mocks --- */

/* Mock texture_load_pixels to avoid real I/O and STB dependencies */
float* texture_load_pixels(const char* path, int* width, int* height,
                           int* channels)
{
	if (strcmp(path, "fail.hdr") == 0) {
		return NULL;
	}
	*width = 16;
	*height = 16;
	*channels = 4;
	return malloc(16 * 16 * 4 * sizeof(float));
}

/* Mock STB */
void stbi_image_free(void* p)
{
	free(p);
}

/* Mock conversion utils */
void convert_float_to_half_simd(const float* src, uint16_t* dst, size_t count)
{
	(void)src;
	(void)dst;
	(void)count;
}

/* Mock log */
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
}

/* Mock tracy_manager functions used by async_loader */
void tracy_manager_async_transition(struct TracyManager* mgr, int state)
{
	(void)mgr;
	(void)state;
}
void tracy_manager_async_end(struct TracyManager* mgr)
{
	(void)mgr;
}

/* GL Mocks for perf_timer.c dependency */
void glGenQueries(GLsizei n, GLuint* ids)
{
	for (int i = 0; i < n; i++) {
		ids[i] = 300 + i;
	}
}
void glDeleteQueries(GLsizei n, const GLuint* ids)
{
	(void)n;
	(void)ids;
}
void glQueryCounter(GLuint id, GLenum target)
{
	(void)id;
	(void)target;
}
void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params)
{
	(void)id;
	(void)pname;
	if (params) {
		*params = 1;
	}
}
void glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params)
{
	(void)id;
	(void)pname;
	if (params) {
		*params = 1000;
	}
}
void glFlush(void)
{
}
void glFinish(void)
{
}

/* GPU Profiler Mocks - Correct signatures to match gpu_profiler.h */
void gpu_profiler_start_stage(GPUProfiler* profiler, const char* name,
                              uint32_t color)
{
	(void)profiler;
	(void)name;
	(void)color;
}
void gpu_profiler_end_stage(GPUProfiler* profiler)
{
	(void)profiler;
}

/* --- Helpers --- */

static void sleep_ms(long milliseconds)
{
	platform_sleep_ms((uint32_t)milliseconds);
}

/* --- Tests --- */

static AsyncLoader* loader;

void setUp(void)
{
	loader = async_loader_create(NULL);
}

void tearDown(void)
{
	async_loader_destroy(loader);
}

void test_AsyncLoader_RequestAndPollSuccess(void)
{
	bool req_ok = async_loader_request(loader, "test.hdr");
	TEST_ASSERT_TRUE(req_ok);

	/* Poll until it's waiting for PBO */
	AsyncRequest out_req = {0};
	bool found = false;
	for (int i = 0; i < 50; ++i) {
		if (async_loader_poll(loader, &out_req)) {
			if (out_req.state == ASYNC_WAITING_FOR_PBO) {
				found = true;
				break;
			}
		}
		sleep_ms(10);
	}
	TEST_ASSERT_TRUE_MESSAGE(found, "Should reach WAITING_FOR_PBO");
	TEST_ASSERT_EQUAL_INT(16, out_req.width);

	/* Provide a dummy PBO/ptr */
	char dummy_pbo_mem[4096];
	async_loader_provide_pbo(loader, dummy_pbo_mem, 123);

	/* Poll until it's ready */
	found = false;
	for (int i = 0; i < 50; ++i) {
		if (async_loader_poll(loader, &out_req)) {
			if (out_req.state == ASYNC_READY) {
				found = true;
				break;
			}
		}
		sleep_ms(10);
	}
	TEST_ASSERT_TRUE_MESSAGE(found, "Should reach READY state");
	TEST_ASSERT_EQUAL_UINT(123, out_req.pbo_id);
}

void test_AsyncLoader_HandleLoadFailure(void)
{
	async_loader_request(loader, "fail.hdr");

	/* Poll should eventually see a reset to IDLE or just never return true
	   The current implementation of poll returns false and resets to IDLE
	   if failed.
	*/
	bool found_ready = false;
	for (int i = 0; i < 50; ++i) {
		AsyncRequest out_req = {0};
		if (async_loader_poll(loader, &out_req)) {
			if (out_req.state == ASYNC_READY) {
				found_ready = true;
			}
		}
		sleep_ms(10);
	}
	TEST_ASSERT_FALSE_MESSAGE(found_ready,
	                          "Should not reach READY on failure");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_AsyncLoader_RequestAndPollSuccess);
	RUN_TEST(test_AsyncLoader_HandleLoadFailure);
	return UNITY_END();
}
