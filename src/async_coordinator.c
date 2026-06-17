#include "async/async_coordinator.h"

#include "app.h"
#include "gl_common.h"
#include "log.h"
#include "platform/platform_utils.h"
#include "profiler.h"
#include "texture.h"
#include <stdlib.h>

void async_coordinator_init(AsyncCoordinator* coord)
{
	if (!coord) {
		return;
	}
	glGenBuffers(2, coord->upload_pbo);
	coord->upload_pbo_idx = 0;
	coord->upload_pbo_size[0] = 0;
	coord->upload_pbo_size[1] = 0;
	coord->pending_prealloc_w = 0;
	coord->pending_prealloc_h = 0;
}

void async_coordinator_cleanup(AsyncCoordinator* coord)
{
	if (!coord) {
		return;
	}
	GL_SAFE_DELETE_BUFFERS(2, coord->upload_pbo);
}

bool async_coordinator_update(AsyncCoordinator* coord, AsyncLoader* loader,
                              AsyncRequest* out_req)
{
	if (!coord || !loader || !out_req) {
		return false;
	}

	bool result = false;
	AsyncRequest req;
	if (async_loader_poll(loader, &req)) {
		if (req.state == ASYNC_WAITING_FOR_PBO) {
			/* Step 1: Main thread provides mapped PBO */
			/* Use ping-pong index to avoid stalling on previous
			 * frame's upload */
			int pbo_idx = coord->upload_pbo_idx;

			/* Consomme la taille pré-calculée par le worker */
			size_t size = req.required_pbo_size;

			PROFILE_ZONE(pbo_ctx, "PBO Setup & Map");
			texture_ensure_pbo(&coord->upload_pbo[pbo_idx],
			                   &coord->upload_pbo_size[pbo_idx],
			                   (GLsizeiptr)size);
			void* ptr =
			    texture_map_pbo(coord->upload_pbo[pbo_idx], size);
			PROFILE_ZONE_END(pbo_ctx);

			if (ptr) {
				async_loader_provide_pbo(
				    loader, ptr, coord->upload_pbo[pbo_idx]);
				coord->upload_pbo_idx =
				    (coord->upload_pbo_idx + 1) % 2;

				coord->pending_prealloc_w = req.width;
				coord->pending_prealloc_h = req.height;
			} else {
				LOG_WARN("suckless-ogl.async",
				         "PBO mapping failed, falling back to "
				         "CPU memory");
				void* fallback_ptr = malloc(size);
				if (fallback_ptr) {
					async_loader_provide_pbo(
					    loader, fallback_ptr, 0);
				} else {
					LOG_ERROR(
					    "suckless-ogl.async",
					    "CPU fallback allocation failed!");
					async_loader_cancel(loader);
				}
			}
		} else if (req.state == ASYNC_READY) {
			/* Request is fully ready to be processed */
			*out_req = req;
			result = true;
		}
	}

	return result;
}

/* --- Subsystem descriptor (Phase 1 alloc + Phase 3 GL init) --- */

/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */
int async_coord_subsys_init(App* app)
{
	app->async_coord =
	    platform_aligned_alloc(sizeof(*app->async_coord), SIMD_ALIGNMENT);
	if (!app->async_coord) {
		return 0;
	}
	*app->async_coord = (AsyncCoordinator){0};
	async_coordinator_init(app->async_coord);
	return 1;
}

/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */
void async_coord_subsys_cleanup(App* app)
{
	if (app->async_coord) {
		async_coordinator_cleanup(app->async_coord);
		platform_aligned_free(app->async_coord);
		app->async_coord = NULL;
	}
}
