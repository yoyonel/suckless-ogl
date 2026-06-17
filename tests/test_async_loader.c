#ifndef GL_COMMON_NO_GLAD
#define GL_COMMON_NO_GLAD
#include "asset_manager.h"
#endif
#ifndef GL_COMMON_NO_GLFW
#define GL_COMMON_NO_GLFW
#endif

#include "async_loader.h"
#include "gl_common.h"
#include "log.h"
#include "platform/platform_time.h"
#include "profiler.h"
#include <ktx.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unity.h>

/* Helper pour forger un handle dans les tests sans dépendre de io/fs */
static AssetHandle make_test_handle(const char* path, AssetType type)
{
	AssetHandle asset_handler = {0};
	strncpy(asset_handler.full_path, path,
	        sizeof(asset_handler.full_path) - 1);
	asset_handler.type = type;
	return asset_handler;
}

/* --- Mocks --- */

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

void stbi_image_free(void* p)
{
	free(p);
}

void convert_float_to_half_simd(const float* src, uint16_t* dst, size_t count)
{
	(void)src;
	(void)dst;
	(void)count;
}

void log_message(LogLevel level, const char* tag, const char* format, ...)
{
	(void)level;
	(void)tag;
	(void)format;
}

void tracy_manager_async_transition(struct TracyManager* mgr, AsyncState state)
{
	(void)mgr;
	(void)state;
}

void tracy_manager_async_end(struct TracyManager* mgr)
{
	(void)mgr;
}

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
	AssetHandle handle =
	    make_test_handle("test.hdr", ASSET_TYPE_TEXTURE_STB);
	bool req_ok = async_loader_request(loader, &handle);
	TEST_ASSERT_TRUE(req_ok);

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

	char dummy_pbo_mem[4096];
	async_loader_provide_pbo(loader, dummy_pbo_mem, 123);

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
	AssetHandle handle =
	    make_test_handle("fail.hdr", ASSET_TYPE_TEXTURE_STB);
	async_loader_request(loader, &handle);

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

void test_AsyncLoader_TaggedUnion_STB(void)
{
	AssetHandle handle =
	    make_test_handle("test.hdr", ASSET_TYPE_TEXTURE_STB);
	bool req_ok = async_loader_request(loader, &handle);
	TEST_ASSERT_TRUE(req_ok);

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

	TEST_ASSERT_TRUE_MESSAGE(
	    found, "La requête n'a pas atteint l'état WAITING_FOR_PBO");

	/* Vérification via le pointeur opaque de contexte du backend */
	TEST_ASSERT_EQUAL_INT(ASSET_TYPE_TEXTURE_STB, out_req.resource_type);
	TEST_ASSERT_NOT_NULL_MESSAGE(
	    out_req.backend_data,
	    "Le backend_data STB doit contenir les données float décodées");

	size_t expected_pbo_size =
	    (size_t)out_req.width * out_req.height * 4 * sizeof(uint16_t);
	TEST_ASSERT_EQUAL_UINT64_MESSAGE(
	    expected_pbo_size, out_req.required_pbo_size,
	    "Le worker doit pré-calculer la taille du PBO requise");
}

void test_AsyncLoader_TaggedUnion_KTX_Routing(void)
{
	AssetHandle handle =
	    make_test_handle("missing_dummy.ktx2", ASSET_TYPE_TEXTURE_KTX);
	bool req_ok = async_loader_request(loader, &handle);
	TEST_ASSERT_TRUE(req_ok);

	AsyncRequest out_req = {0};
	bool found_failed = false;

	for (int i = 0; i < 50; ++i) {
		if (async_loader_poll(loader, &out_req)) {
			if (out_req.state == ASYNC_FAILED) {
				found_failed = true;
				break;
			}
		}
		sleep_ms(10);
	}

	TEST_ASSERT_TRUE_MESSAGE(
	    found_failed, "La requête aurait dû échouer (fichier inexistant)");

	TEST_ASSERT_EQUAL_INT(ASSET_TYPE_TEXTURE_KTX, out_req.resource_type);
}

void test_AsyncLoader_PerformConversion_KTX(void)
{
	const char* dummy_file = "test_conversion.ktx2";

	ktxTextureCreateInfo createInfo = {0};
	createInfo.vkFormat = 43; /* VK_FORMAT_R8G8B8A8_UNORM */
	createInfo.baseWidth = 2;
	createInfo.baseHeight = 2;
	createInfo.baseDepth = 1;
	createInfo.numDimensions = 2;
	createInfo.numLevels = 1;
	createInfo.numLayers = 1;
	createInfo.numFaces = 1;

	ktxTexture2* out_tex = NULL;
	KTX_error_code res = ktxTexture2_Create(
	    &createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &out_tex);
	TEST_ASSERT_EQUAL_INT(KTX_SUCCESS, res);

	uint8_t src_pixels[16] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22,
	                          0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	                          0x99, 0x00, 0xAA, 0xFF};

	ktxTexture_SetImageFromMemory((ktxTexture*)out_tex, 0, 0, 0, src_pixels,
	                              sizeof(src_pixels));
	ktxTexture_WriteToNamedFile((ktxTexture*)out_tex, dummy_file);
	ktxTexture_Destroy((ktxTexture*)out_tex);

	AssetHandle handle =
	    make_test_handle(dummy_file, ASSET_TYPE_TEXTURE_KTX);
	TEST_ASSERT_TRUE(async_loader_request(loader, &handle));

	AsyncRequest req = {0};
	bool waiting = false;
	for (int i = 0; i < 50; ++i) {
		if (async_loader_poll(loader, &req) &&
		    req.state == ASYNC_WAITING_FOR_PBO) {
			waiting = true;
			break;
		}
	subsys_lock_retry:
		sleep_ms(10);
	}

	TEST_ASSERT_TRUE_MESSAGE(waiting, "Le loader devrait attendre le PBO");
	TEST_ASSERT_EQUAL_INT(ASSET_TYPE_TEXTURE_KTX, req.resource_type);
	TEST_ASSERT_EQUAL_UINT64(16, req.required_pbo_size);

	uint8_t dummy_pbo[16] = {0};
	async_loader_provide_pbo(loader, dummy_pbo, 999);

	bool ready = false;
	for (int i = 0; i < 50; ++i) {
		if (async_loader_poll(loader, &req) &&
		    req.state == ASYNC_READY) {
			ready = true;
			break;
		}
		sleep_ms(10);
	}

	TEST_ASSERT_TRUE_MESSAGE(
	    ready, "Le loader devrait passer en ASYNC_READY après la copie");

	TEST_ASSERT_EQUAL_UINT8_ARRAY(src_pixels, dummy_pbo, 16);

	remove(dummy_file);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_AsyncLoader_RequestAndPollSuccess);
	RUN_TEST(test_AsyncLoader_HandleLoadFailure);
	RUN_TEST(test_AsyncLoader_TaggedUnion_STB);
	RUN_TEST(test_AsyncLoader_TaggedUnion_KTX_Routing);
	RUN_TEST(test_AsyncLoader_PerformConversion_KTX);
	return UNITY_END();
}
