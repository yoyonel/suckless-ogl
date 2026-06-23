#include "async_backend.h"
#include "async_loader.h"
#include "gl_common.h"
#include "perf_timer.h"
#include "profiler.h"
#include "simd_utils.h"
#include "texture.h"
#include "utils.h"
#include <stb_image.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool stb_load(const char* path, AsyncRequest* req)
{
	PROFILE_ZONE(work_ctx, "I/O & Decoding");
	PROFILE_MESSAGE(path, strlen(path));
	PerfTimer disk_timer;
	perf_timer_start(&disk_timer);

	int width = 0;
	int height = 0;
	int channels = 0;

	float* data = texture_load_pixels(path, &width, &height, &channels);
	if (!data) {
		PROFILE_ZONE_END(work_ctx);
		return false;
	}

	req->backend_data = data;
	req->required_pbo_size =
	    (size_t)width * (size_t)height * 4 * sizeof(uint16_t);
	req->width = width;
	req->height = height;
	req->channels = channels;

	req->is_compressed = false;
	req->gl_internal_format = GL_RGBA16F;
	req->gl_format = GL_RGBA;
	req->gl_type = GL_HALF_FLOAT;

	double load_ms = perf_timer_elapsed_ms(&disk_timer);
	char msg[MSG_BUF_SIZE];
	int res = safe_snprintf(msg, sizeof(msg), "Load: %.2f ms", load_ms);
	if (res >= 0) {
		PROFILE_MESSAGE(msg, (size_t)res);
	}
	PROFILE_ZONE_END(work_ctx);

	return true;
}

static void stb_convert(void* dst_ptr, AsyncRequest* req)
{
	PROFILE_ZONE(conv_ctx, "Float->Half Convert (STB)");
	float* src_data = (float*)req->backend_data;
	size_t pixel_count = (size_t)req->width * (size_t)req->height * 4;

	if (dst_ptr && src_data) {
		convert_float_to_half_simd(src_data, (uint16_t*)dst_ptr,
		                           pixel_count);
	}
	if (src_data) {
		stbi_image_free(src_data);
	}
	PROFILE_ZONE_END(conv_ctx);
}

static void stb_cleanup(AsyncRequest* req)
{
	if (req->backend_data) {
		stbi_image_free((float*)req->backend_data);
		req->backend_data = NULL;
	}
}

/* La table devient privée à l'unité de traduction */
static const AsyncBackendInterface g_backend_stb = {stb_load, stb_convert,
                                                    stb_cleanup};

/* Point d'accès public externe */
const AsyncBackendInterface* async_backend_stb_get(void)
{
	return &g_backend_stb;
}
