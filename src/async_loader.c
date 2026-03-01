#include "async_loader.h"

#include "log.h"
#include "perf_timer.h"
#include "simd_utils.h"
#include "texture.h"
#include "tracy_manager.h"
#include "utils.h"
#include <pthread.h>
#include <stb_image.h>
#include <stdbool.h>
#include <string.h>

enum { MSG_BUF_SIZE = 128 };

struct AsyncLoader {
	AsyncRequest current_request;
	pthread_mutex_t request_mutex;
	pthread_cond_t request_cond;
	pthread_t worker_thread;
	volatile bool running;
	volatile bool has_pending_work;
	PerfTimer sys_timer;
	TracyManager* tracy_mgr;
};

#define transition_tracy_state(s) \
	tracy_manager_async_transition(loader->tracy_mgr, s)
#define cleanup_tracy_states() tracy_manager_async_end(loader->tracy_mgr)

static bool async_load_data(const char* path, float** out_data, int* width,
                            int* height, int* channels)
{
#ifdef TRACY_ENABLE
	TracyCZoneN(work_ctx, "I/O & Docoding", 1);
	TracyCMessage(path, strlen(path));
#endif
	PerfTimer disk_timer;
	perf_timer_start(&disk_timer);
	*out_data = texture_load_pixels(path, width, height, channels);
#ifdef TRACY_ENABLE
	double load_ms = perf_timer_elapsed_ms(&disk_timer);
	char msg[MSG_BUF_SIZE];
	int res = safe_snprintf(msg, sizeof(msg), "Load: %.2f ms", load_ms);
	if (res >= 0) {
		TracyCMessage(msg, (size_t)res);
	}
	TracyCZoneEnd(work_ctx);
#endif
	return *out_data != NULL;
}

static void async_perform_conversion(AsyncLoader* loader)
{
	/* Unlock mutex to allow main thread to poll without blocking! */
	float* src_data = loader->current_request.float_data;
	void* dst_ptr = loader->current_request.pbo_mapped_ptr;
	int width = loader->current_request.width;
	int height = loader->current_request.height;

	pthread_mutex_unlock(&loader->request_mutex);

#ifdef TRACY_ENABLE
	TracyCZoneN(conv_ctx, "Float->Half Convert", 1);
#endif
	size_t pixel_count = (size_t)width * (size_t)height * 4;

	/* Perform conversion directly into mapped PBO memory */
	if (dst_ptr && src_data) {
		convert_float_to_half_simd(src_data, (uint16_t*)dst_ptr,
		                           pixel_count);
	}

	/* Free CPU float data */
	if (src_data) {
		stbi_image_free(src_data);
	}

#ifdef TRACY_ENABLE
	TracyCZoneEnd(conv_ctx);
#endif
	/* Re-acquire mutex to update state */
	pthread_mutex_lock(&loader->request_mutex);

	loader->current_request.float_data = NULL; /* Marked as freed */
	loader->current_request.state = ASYNC_READY;
	transition_tracy_state(ASYNC_READY);
	LOG_INFO("suckless-ogl.async", "Finished loading & converting: %s",
	         loader->current_request.path);
}

static void async_handle_io(AsyncLoader* loader, char* path_to_load)
{
	int width = 0;
	int height = 0;
	int channels = 0;
	float* data = NULL;

	/* 1. Heavy I/O */
	if (!async_load_data(path_to_load, &data, &width, &height, &channels)) {
		pthread_mutex_lock(&loader->request_mutex);
		loader->current_request.state = ASYNC_FAILED;
		transition_tracy_state(ASYNC_FAILED);
		LOG_ERROR("suckless-ogl.async", "Failed loading: %s",
		          path_to_load);
		return;
	}

	/* 2. Update state to WAITING_FOR_PBO */
	pthread_mutex_lock(&loader->request_mutex);
	loader->current_request.float_data = data;
	loader->current_request.width = width;
	loader->current_request.height = height;
	loader->current_request.channels = channels;
	loader->current_request.state = ASYNC_WAITING_FOR_PBO;
	transition_tracy_state(ASYNC_WAITING_FOR_PBO);

	/* 3. Wait for PBO */
	while (loader->running &&
	       loader->current_request.state == ASYNC_WAITING_FOR_PBO) {
		pthread_cond_wait(&loader->request_cond,
		                  &loader->request_mutex);
	}

	/* 4. Convert or Clean up */
	if (!loader->running ||
	    loader->current_request.state != ASYNC_CONVERTING) {
		/* Cancelled or failed */
		if (loader->current_request.float_data) {
			stbi_image_free(loader->current_request.float_data);
			loader->current_request.float_data = NULL;
		}
		loader->current_request.state = ASYNC_FAILED;
		transition_tracy_state(ASYNC_FAILED);
	} else {
		/* 5. Convert (Mutex is unlocked inside) */
		async_perform_conversion(loader);
	}
}

static void* async_worker_func(void* arg)
{
	AsyncLoader* loader = (AsyncLoader*)arg;

#ifdef TRACY_ENABLE
	TracyCSetThreadName("Async Loader");
#endif

	pthread_mutex_lock(&loader->request_mutex);
	while (loader->running) {
		while (loader->running && !loader->has_pending_work) {
#ifdef TRACY_ENABLE
			TracyCMessageL("Waiting for work...");
#endif
			pthread_cond_wait(&loader->request_cond,
			                  &loader->request_mutex);
		}

		if (!loader->running) {
			break;
		}

		/* Extract work details */
		char path_to_load[ASYNC_MAX_PATH];
		bool has_work = false;

		if (loader->current_request.state == ASYNC_PENDING) {
			(void)safe_snprintf(path_to_load, sizeof(path_to_load),
			                    "%s", loader->current_request.path);
			loader->current_request.state = ASYNC_LOADING;
			transition_tracy_state(ASYNC_LOADING);

#ifdef TRACY_ENABLE
			double now = perf_timer_elapsed_ms(&loader->sys_timer);
			double queue_time =
			    now - loader->current_request.submission_time;
			char msg[MSG_BUF_SIZE];
			int res =
			    safe_snprintf(msg, sizeof(msg),
			                  "Queuing delay: %.2f ms", queue_time);
			if (res >= 0) {
				TracyCMessage(msg, (size_t)res);
			}
#endif
			has_work = true;
		}

		/* Unlock to perform heavy work */
		pthread_mutex_unlock(&loader->request_mutex);

		if (has_work) {
			async_handle_io(loader, path_to_load);
			/* async_handle_io returns with mutex HELD in all
			 * paths (success, failure, cancel). No re-lock needed.
			 */
		} else {
			/* No work was dispatched, re-acquire for next
			 * iteration */
			pthread_mutex_lock(&loader->request_mutex);
		}
		loader->has_pending_work = false;
	}
	pthread_mutex_unlock(&loader->request_mutex);
	return NULL;
}

AsyncLoader* async_loader_create(struct TracyManager* mgr)
{
	AsyncLoader* loader = (AsyncLoader*)calloc(1, sizeof(AsyncLoader));
	if (!loader) {
		LOG_ERROR("suckless-ogl.async", "Failed to allocate loader");
		return NULL;
	}

	(void)safe_memset(&loader->current_request, sizeof(AsyncRequest), 0,
	                  sizeof(AsyncRequest));
	loader->current_request.state = ASYNC_IDLE;

	if (pthread_mutex_init(&loader->request_mutex, NULL) != 0) {
		LOG_ERROR("suckless-ogl.async", "Mutex init failed");
		free(loader);
		return NULL;
	}

	if (pthread_cond_init(&loader->request_cond, NULL) != 0) {
		LOG_ERROR("suckless-ogl.async", "Cond init failed");
		pthread_mutex_destroy(&loader->request_mutex);
		free(loader);
		return NULL;
	}

	loader->running = true;
	loader->tracy_mgr = mgr;
	perf_timer_start(&loader->sys_timer);
	transition_tracy_state(ASYNC_IDLE);

	if (pthread_create(&loader->worker_thread, NULL, async_worker_func,
	                   loader) != 0) {
		LOG_ERROR("suckless-ogl.async", "Thread creation failed");
		loader->running = false;
		pthread_cond_destroy(&loader->request_cond);
		pthread_mutex_destroy(&loader->request_mutex);
		free(loader);
		return NULL;
	}

	LOG_INFO("suckless-ogl.async", "Async loader initialized.");
	return loader;
}

void async_loader_destroy(AsyncLoader* loader)
{
	if (!loader) {
		return;
	}

	if (loader->running) {
		pthread_mutex_lock(&loader->request_mutex);
		loader->running = false;
		pthread_cond_broadcast(&loader->request_cond);
		pthread_mutex_unlock(&loader->request_mutex);

		pthread_join(loader->worker_thread, NULL);
	}

	/* Cleanup any pending request data that wasn't consumed */
	if (loader->current_request.half_data) {
		free(loader->current_request.half_data);
		loader->current_request.half_data = NULL;
	}
	if (loader->current_request.float_data) {
		stbi_image_free(loader->current_request.float_data);
		loader->current_request.float_data = NULL;
	}

	pthread_cond_destroy(&loader->request_cond);
	pthread_mutex_destroy(&loader->request_mutex);
	cleanup_tracy_states();
	free(loader);

	LOG_INFO("suckless-ogl.async", "Async loader destroyed.");
}

bool async_loader_request(AsyncLoader* loader, const char* path)
{
#ifdef TRACY_ENABLE
	TracyCZoneN(req_ctx, "async_loader_request", 1);
	TracyCMessage(path, path ? strlen(path) : 0);
#endif
	if (!loader || !path) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(req_ctx);
#endif
		return false;
	}

	bool accepted = false;
#ifdef TRACY_ENABLE
	TracyCZoneN(mtx_ctx, "Request Mutex Lock", 1);
#endif
	pthread_mutex_lock(&loader->request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(mtx_ctx);
#endif

	/* Only accept if idle or failed (retry) */
	if (loader->current_request.state == ASYNC_IDLE ||
	    loader->current_request.state == ASYNC_FAILED ||
	    loader->current_request.state == ASYNC_READY) {
		/* Cleanup previous result if it wasn't consumed */
		if (loader->current_request.half_data) {
			free(loader->current_request.half_data);
			loader->current_request.half_data = NULL;
		}
		if (loader->current_request.float_data) {
			stbi_image_free(loader->current_request.float_data);
			loader->current_request.float_data = NULL;
		}

		if (safe_snprintf(loader->current_request.path,
		                  sizeof(loader->current_request.path), "%s",
		                  path) < 0) {
			LOG_ERROR("suckless-ogl.async", "Path too long: %s",
			          path);
			loader->current_request.state = ASYNC_IDLE;
			transition_tracy_state(ASYNC_IDLE);
		} else {
			loader->current_request.state = ASYNC_PENDING;
			loader->current_request.submission_time =
			    perf_timer_elapsed_ms(&loader->sys_timer);
			transition_tracy_state(ASYNC_PENDING);
			loader->has_pending_work = true;
			pthread_cond_signal(&loader->request_cond);
			accepted = true;
		}
	}

	pthread_mutex_unlock(&loader->request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(req_ctx);
#endif
	return accepted;
}

bool async_loader_poll(AsyncLoader* loader, AsyncRequest* out_req)
{
#ifdef TRACY_ENABLE
	TracyCZoneN(poll_ctx, "async_loader_poll", 1);
#endif
	if (!loader || !out_req) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(poll_ctx);
#endif
		return false;
	}

	bool result = false;
#ifdef TRACY_ENABLE
	TracyCZoneN(mtx_ctx, "Poll Mutex Lock", 1);
#endif
	pthread_mutex_lock(&loader->request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(mtx_ctx);
#endif

	if (loader->current_request.state == ASYNC_READY ||
	    loader->current_request.state == ASYNC_WAITING_FOR_PBO) {
		/* Copy result to caller */
		*out_req = loader->current_request;

		if (loader->current_request.state == ASYNC_READY) {
			/* Only clear if fully ready */
			loader->current_request.state = ASYNC_IDLE;
			transition_tracy_state(ASYNC_IDLE);
			loader->current_request.half_data =
			    NULL; /* Ownership transferred */
			loader->current_request.pbo_mapped_ptr = NULL;
		}
		/* If WAITING_FOR_PBO, we just return true with state,
		   but don't clear internal state yet. Main thread must act. */

		result = true;
	} else if (loader->current_request.state == ASYNC_FAILED) {
		/* Failed loading, just reset */
		LOG_ERROR("suckless-ogl.async", "Async load failed for: %s",
		          loader->current_request.path);
		loader->current_request.state = ASYNC_IDLE;
		transition_tracy_state(ASYNC_IDLE);
	}

	pthread_mutex_unlock(&loader->request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(poll_ctx);
#endif
	return result;
}

void async_loader_provide_pbo(AsyncLoader* loader, void* mapped_ptr,
                              GLuint pbo_id)
{
	if (!loader) {
		return;
	}
#ifdef TRACY_ENABLE
	TracyCZoneN(ctx, "async_loader_provide_pbo", 1);
	TracyCZoneN(mtx_ctx, "Provide PBO Mutex Lock", 1);
#endif
	pthread_mutex_lock(&loader->request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(mtx_ctx);
#endif

	if (loader->current_request.state == ASYNC_WAITING_FOR_PBO) {
		loader->current_request.pbo_mapped_ptr = mapped_ptr;
		loader->current_request.pbo_id = pbo_id;
		loader->current_request.state = ASYNC_CONVERTING;
		transition_tracy_state(ASYNC_CONVERTING);
		pthread_cond_signal(&loader->request_cond); /* Wake up worker */
	} else {
		LOG_ERROR("suckless-ogl.async",
		          "Main thread provided PBO but loader not waiting "
		          "(State: %d)",
		          loader->current_request.state);
	}

	pthread_mutex_unlock(&loader->request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(ctx);
#endif
}

void async_loader_cancel(AsyncLoader* loader)
{
	if (!loader) {
		return;
	}
#ifdef TRACY_ENABLE
	TracyCZoneN(ctx, "async_loader_cancel", 1);
	TracyCZoneN(mtx_ctx, "Cancel Mutex Lock", 1);
#endif
	pthread_mutex_lock(&loader->request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(mtx_ctx);
#endif

	if (loader->current_request.state == ASYNC_WAITING_FOR_PBO) {
		/* Set state to FAILED to break the worker loop */
		loader->current_request.state = ASYNC_FAILED;
		transition_tracy_state(ASYNC_FAILED);
		pthread_cond_signal(&loader->request_cond); /* Wake up worker */
		LOG_INFO("suckless-ogl.async",
		         "Request cancelled by main thread: %s",
		         loader->current_request.path);
	} else {
		LOG_ERROR("suckless-ogl.async",
		          "Attempted to cancel request but state is %d",
		          loader->current_request.state);
	}

	pthread_mutex_unlock(&loader->request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(ctx);
#endif
}
