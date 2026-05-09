#include "tracy_manager.h"

#include "app.h"

#ifdef TRACY_ENABLE

#include "mem.h"
#include "profiler.h"
#include "render_utils.h"
#include <cJSON.h>
#include <pthread.h>

void tracy_manager_init_global(void)
{
	cJSON_Hooks hooks;
	hooks.malloc_fn = tracy_malloc;
	hooks.free_fn = tracy_free;
	cJSON_InitHooks(&hooks);
}

void tracy_manager_init(TracyManager* mgr, int width, int height)
{
	(void)width;
	(void)height;
	tracy_gpu_init();

	mgr->screenshot_tex = render_utils_create_texture_2d(
	    TRACY_SCREENSHOT_WIDTH, TRACY_SCREENSHOT_HEIGHT, GL_RGBA8, 1,
	    "Tracy Screenshot");
	glGenFramebuffers(1, &mgr->screenshot_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mgr->screenshot_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, mgr->screenshot_tex, 0);
	render_utils_check_framebuffer("Tracy Screenshot FBO");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenBuffers(TRACY_PBO_COUNT, mgr->screenshot_pbo);
	for (int i = 0; i < TRACY_PBO_COUNT; i++) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, mgr->screenshot_pbo[i]);
		glBufferData(
		    GL_PIXEL_PACK_BUFFER,
		    TRACY_SCREENSHOT_WIDTH * TRACY_SCREENSHOT_HEIGHT * 4, NULL,
		    GL_STREAM_READ);
		mgr->screenshot_sync[i] = 0;
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	mgr->screenshot_pbo_idx = 0;

	/* Initialize encapsulated state */
	mgr->active_state_ctx.id = 0;
	pthread_mutex_init(&mgr->transition_mutex, NULL);
}

void tracy_manager_cleanup(TracyManager* mgr)
{
	glDeleteTextures(1, &mgr->screenshot_tex);
	glDeleteFramebuffers(1, &mgr->screenshot_fbo);
	glDeleteBuffers(TRACY_PBO_COUNT, mgr->screenshot_pbo);
	for (int i = 0; i < TRACY_PBO_COUNT; i++) {
		if (mgr->screenshot_sync[i]) {
			glDeleteSync(mgr->screenshot_sync[i]);
			mgr->screenshot_sync[i] = 0;
		}
	}

	tracy_manager_async_end(mgr);
	pthread_mutex_destroy(&mgr->transition_mutex);
}

void tracy_manager_update_screenshots(TracyManager* mgr, App* app)
{
	PROFILE_ZONE(ctx, "Tracy Screenshot Update");
	/* 1. Send previous frame's screenshot (already in PBO) */
	int read_idx = (mgr->screenshot_pbo_idx + 1) % TRACY_PBO_COUNT;
	bool ready_to_read = false;

	if (mgr->screenshot_sync[read_idx]) {
		GLenum wait_res =
		    glClientWaitSync(mgr->screenshot_sync[read_idx],
		                     GL_SYNC_FLUSH_COMMANDS_BIT, 0);
		if (wait_res == GL_TIMEOUT_EXPIRED ||
		    wait_res == GL_WAIT_FAILED) {
			ready_to_read = false;
			PROFILE_MESSAGE_C("Screenshot skip (GPU stall)", 27,
			                  0xFFAA00);
		} else {
			ready_to_read = true;
			glDeleteSync(mgr->screenshot_sync[read_idx]);
			mgr->screenshot_sync[read_idx] = 0;
		}
	}

	if (ready_to_read) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER,
		             mgr->screenshot_pbo[read_idx]);
		void* pbo_ptr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		if (pbo_ptr) {
			tracy_gpu_screenshot(pbo_ptr, TRACY_SCREENSHOT_WIDTH,
			                     TRACY_SCREENSHOT_HEIGHT);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		}
	}

	/* 2. Start new screenshot capture for current frame */
	glDisable(GL_SCISSOR_TEST);

	/* First downscale the backbuffer to our small FBO */
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mgr->screenshot_fbo);
	glBlitFramebuffer(0, 0, app->width, app->height, 0, 0,
	                  TRACY_SCREENSHOT_WIDTH, TRACY_SCREENSHOT_HEIGHT,
	                  GL_COLOR_BUFFER_BIT, GL_LINEAR);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, mgr->screenshot_fbo);
	glBindBuffer(GL_PIXEL_PACK_BUFFER,
	             mgr->screenshot_pbo[mgr->screenshot_pbo_idx]);
	glReadPixels(0, 0, TRACY_SCREENSHOT_WIDTH, TRACY_SCREENSHOT_HEIGHT,
	             GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	/* Create sync object before moving to next frame */
	if (mgr->screenshot_sync[mgr->screenshot_pbo_idx]) {
		glDeleteSync(mgr->screenshot_sync[mgr->screenshot_pbo_idx]);
	}
	mgr->screenshot_sync[mgr->screenshot_pbo_idx] =
	    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	/* 3. Ping-pong */
	mgr->screenshot_pbo_idx =
	    (mgr->screenshot_pbo_idx + 1) % TRACY_PBO_COUNT;
	PROFILE_ZONE_END(ctx);
}

void tracy_manager_async_transition(TracyManager* mgr, AsyncState new_state)
{
	const int active = 1;
	const uint32_t color_idle = 0x888888;
	const uint32_t color_pending = 0xAAAA00;
	const uint32_t color_loading = 0x00AA00;
	const uint32_t color_ready = 0x00FFAA;
	const uint32_t color_failed = 0xFF0000;

	pthread_mutex_lock(&mgr->transition_mutex);
	PROFILE_FIBER_ENTER("Async Status");
	if (mgr->active_state_ctx.id != 0) {
		PROFILE_ZONE_END(mgr->active_state_ctx);
		mgr->active_state_ctx.id = 0;
	}

	switch (new_state) {
		case ASYNC_IDLE: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async IDLE", __func__, TracyFile,
			              (uint32_t)__LINE__, color_idle};
			mgr->active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_PENDING: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async PENDING", __func__, TracyFile,
			              (uint32_t)__LINE__, color_pending};
			mgr->active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_LOADING: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async LOADING", __func__, TracyFile,
			              (uint32_t)__LINE__, color_loading};
			mgr->active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_WAITING_FOR_PBO: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async WAIT_PBO", __func__, TracyFile,
			              (uint32_t)__LINE__, color_pending};
			mgr->active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_CONVERTING: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async CONVERT", __func__, TracyFile,
			              (uint32_t)__LINE__, color_loading};
			mgr->active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_READY: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async READY", __func__, TracyFile,
			              (uint32_t)__LINE__, color_ready};
			mgr->active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_FAILED: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async FAILED", __func__, TracyFile,
			              (uint32_t)__LINE__, color_failed};
			mgr->active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		default: {
			/* Fallback for unknown states */
			static const struct ___tracy_source_location_data
			    srcloc = {"Async UNKNOWN", __func__, TracyFile,
			              (uint32_t)__LINE__, color_idle};
			mgr->active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
	}
	PROFILE_FIBER_LEAVE;
	pthread_mutex_unlock(&mgr->transition_mutex);
}

void tracy_manager_async_end(TracyManager* mgr)
{
	pthread_mutex_lock(&mgr->transition_mutex);
	if (mgr->active_state_ctx.id != 0) {
		PROFILE_FIBER_ENTER("Async Status");
		PROFILE_ZONE_END(mgr->active_state_ctx);
		mgr->active_state_ctx.id = 0;
		PROFILE_FIBER_LEAVE;
	}
	pthread_mutex_unlock(&mgr->transition_mutex);
}

#else

void tracy_manager_init_global(void)
{
}

void tracy_manager_init(TracyManager* mgr, int width, int height)
{
	(void)mgr;
	(void)width;
	(void)height;
}

void tracy_manager_cleanup(TracyManager* mgr)
{
	(void)mgr;
}

void tracy_manager_update_screenshots(TracyManager* mgr, App* app)
{
	(void)mgr;
	(void)app;
}

void tracy_manager_async_transition(TracyManager* mgr, AsyncState new_state)
{
	(void)mgr;
	(void)new_state;
}

void tracy_manager_async_end(TracyManager* mgr)
{
	(void)mgr;
}

#endif
