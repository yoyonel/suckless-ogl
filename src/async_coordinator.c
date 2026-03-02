#include "async/async_coordinator.h"

#include "gl_common.h"
#include "log.h"
#include "texture.h"

#ifdef TRACY_ENABLE
#include "../deps/tracy/public/tracy/TracyC.h"
#endif

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
			size_t size = (size_t)req.width * (size_t)req.height *
			              4 * sizeof(uint16_t);
#ifdef TRACY_ENABLE
			TracyCZoneN(pbo_ctx, "PBO Setup & Map", 1);
#endif
			texture_ensure_pbo(&coord->upload_pbo[pbo_idx],
			                   &coord->upload_pbo_size[pbo_idx],
			                   (GLsizeiptr)size);
			void* ptr =
			    texture_map_pbo(coord->upload_pbo[pbo_idx], size);
#ifdef TRACY_ENABLE
			TracyCZoneEnd(pbo_ctx);
#endif
			if (ptr) {
				async_loader_provide_pbo(
				    loader, ptr, coord->upload_pbo[pbo_idx]);
				/* Advance index for next request */
				coord->upload_pbo_idx =
				    (coord->upload_pbo_idx + 1) % 2;

				/* Let caller know that we expect a
				 * pre-allocation cost in the near future */
				coord->pending_prealloc_w = req.width;
				coord->pending_prealloc_h = req.height;
			} else {
				LOG_ERROR("suckless-ogl.async",
				          "Failed to map PBO for async upload");
				async_loader_cancel(loader);
			}
		} else if (req.state == ASYNC_READY) {
			/* Request is fully ready to be processed */
			*out_req = req;
			result = true;
		}
	}

	return result;
}
