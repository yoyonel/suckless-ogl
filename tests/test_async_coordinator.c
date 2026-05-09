#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Include GLAD and Log headers from the project */
#include "async/async_coordinator.h"
#include "glad/glad.h"
#include "log.h"
#include "unity.h"

/* --- Mock GLAD Function Pointers --- */
/* src/async_coordinator.c uses glGenBuffers and glDeleteBuffers.
   GLAD defines these as macros: #define glGenBuffers glad_glGenBuffers
   So we need to provide the actual glad_... function pointers.
*/

static void mock_glGenBuffers(GLsizei n, GLuint* buffers)
{
	(void)n;
	if (buffers)
		*buffers = 100; /* Default mock ID */
}

static void mock_glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
	(void)n;
	(void)buffers;
}

/* Define the glad pointers that the linker is looking for */
PFNGLGENBUFFERSPROC glad_glGenBuffers = mock_glGenBuffers;
PFNGLDELETESYNCPROC glad_glDeleteSync =
    NULL; /* Not used in coordinator yet but might be in headers */
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = mock_glDeleteBuffers;

/* --- Mock Log --- */
void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
}

/* --- Mock AsyncLoader --- */
typedef struct AsyncLoader {
	AsyncRequest current_req;
	bool has_req;
	bool pbo_provided;
	void* provided_ptr;
	GLuint provided_pbo;
	bool cancelled;
} AsyncLoader;

AsyncLoader* async_loader_create(struct TracyManager* mgr)
{
	(void)mgr;
	AsyncLoader* loader = calloc(1, sizeof(AsyncLoader));
	return loader;
}

void async_loader_destroy(AsyncLoader* loader)
{
	free(loader);
}

bool async_loader_poll(AsyncLoader* loader, AsyncRequest* out_req)
{
	if (loader->has_req) {
		*out_req = loader->current_req;
		loader->has_req = false;
		return true;
	}
	return false;
}

void async_loader_provide_pbo(AsyncLoader* loader, void* mapped_ptr,
                              GLuint pbo_id)
{
	loader->pbo_provided = true;
	loader->provided_ptr = mapped_ptr;
	loader->provided_pbo = pbo_id;
}

void async_loader_cancel(AsyncLoader* loader)
{
	loader->cancelled = true;
}

/* --- Mock Texture Utils --- */
void texture_ensure_pbo(GLuint* pbo, GLsizeiptr* current_size,
                        GLsizeiptr required_size)
{
	(void)pbo;
	*current_size = required_size;
}

void* texture_map_pbo(GLuint pbo, size_t size)
{
	(void)pbo;
	(void)size;
	static char dummy_buffer[1024];
	if (pbo == 666)
		return NULL; /* Simulate failure */
	return dummy_buffer;
}

/* --- Unity Tests --- */
void setUp(void)
{
}

void tearDown(void)
{
}

void test_AsyncCoordinator_Init_ShouldSetDefaults(void)
{
	AsyncCoordinator coord;
	async_coordinator_init(&coord);

	TEST_ASSERT_EQUAL_INT(0, coord.upload_pbo_idx);
	TEST_ASSERT_EQUAL_INT(0, coord.pending_prealloc_w);
	/* mock_glGenBuffers returns 100 */
	TEST_ASSERT_EQUAL_UINT(100, coord.upload_pbo[0]);
}

void test_AsyncCoordinator_Update_ShouldHandlePBORequest(void)
{
	AsyncCoordinator coord = {0};
	AsyncLoader* loader = async_loader_create(NULL);
	AsyncRequest out_req = {0};

	coord.upload_pbo[0] = 101;
	coord.upload_pbo[1] = 102;
	coord.upload_pbo_idx = 0;

	loader->has_req = true;
	loader->current_req.state = ASYNC_WAITING_FOR_PBO;
	loader->current_req.width = 128;
	loader->current_req.height = 128;

	bool ready = async_coordinator_update(&coord, loader, &out_req);

	TEST_ASSERT_FALSE(ready);
	TEST_ASSERT_TRUE(loader->pbo_provided);
	TEST_ASSERT_EQUAL_UINT(101, loader->provided_pbo);
	TEST_ASSERT_EQUAL_INT(128, coord.pending_prealloc_w);
	TEST_ASSERT_EQUAL_INT(1, coord.upload_pbo_idx);

	async_loader_destroy(loader);
}

void test_AsyncCoordinator_Update_ShouldHandleReadyRequest(void)
{
	AsyncCoordinator coord = {0};
	AsyncLoader* loader = async_loader_create(NULL);
	AsyncRequest out_req = {0};

	loader->has_req = true;
	loader->current_req.state = ASYNC_READY;
	loader->current_req.width = 256;
	strcpy(loader->current_req.path, "test.hdr");

	bool ready = async_coordinator_update(&coord, loader, &out_req);

	TEST_ASSERT_TRUE(ready);
	TEST_ASSERT_EQUAL_INT(256, out_req.width);
	TEST_ASSERT_EQUAL_STRING("test.hdr", out_req.path);

	async_loader_destroy(loader);
}

void test_AsyncCoordinator_Update_ShouldHandleMapFallback(void)
{
	AsyncCoordinator coord = {0};
	AsyncLoader* loader = async_loader_create(NULL);
	AsyncRequest out_req = {0};

	coord.upload_pbo[0] = 666; /* Magic ID to trigger mock map failure */
	coord.upload_pbo_idx = 0;

	loader->has_req = true;
	loader->current_req.state = ASYNC_WAITING_FOR_PBO;
	loader->current_req.width = 128;
	loader->current_req.height = 128;

	bool ready = async_coordinator_update(&coord, loader, &out_req);

	TEST_ASSERT_FALSE(ready);
	/* Should fall back to CPU memory */
	TEST_ASSERT_TRUE(loader->pbo_provided);
	TEST_ASSERT_EQUAL_UINT(0, loader->provided_pbo);
	TEST_ASSERT_NOT_NULL(loader->provided_ptr);
	TEST_ASSERT_FALSE(loader->cancelled);

	/* Clean up fallback pointer if it was allocated */
	if (loader->provided_ptr) {
		free(loader->provided_ptr);
	}

	async_loader_destroy(loader);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_AsyncCoordinator_Init_ShouldSetDefaults);
	RUN_TEST(test_AsyncCoordinator_Update_ShouldHandlePBORequest);
	RUN_TEST(test_AsyncCoordinator_Update_ShouldHandleReadyRequest);
	RUN_TEST(test_AsyncCoordinator_Update_ShouldHandleMapFallback);
	return UNITY_END();
}
