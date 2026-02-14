#include "utils.h"
#define _POSIX_C_SOURCE 200809L  // NOLINT(cert-dcl37-c,cert-dcl51-cpp)
#include "async_loader.h"
#include "log.h"
#include "perf_timer.h"
#include "texture.h"
#include <pthread.h>
#include <stb_image.h>
#include <stdbool.h>
#include <string.h>
#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"
#endif

/* Single slot for now, as we only load one environment map at a time */
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static AsyncRequest current_request;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,misc-include-cleaner)
static pthread_mutex_t request_mutex;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,misc-include-cleaner)
static pthread_cond_t request_cond;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,misc-include-cleaner)
static pthread_t worker_thread;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile bool running = false;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile bool has_pending_work = false;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static PerfTimer loader_sys_timer;

#ifdef TRACY_ENABLE
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static TracyCZoneCtx active_state_ctx;
#define ASYNC_STATE_COUNT 5

static void transition_tracy_state(AsyncState new_state)
{
	const int active = 1;
	const uint32_t color_idle = 0x888888;
	const uint32_t color_pending = 0xAAAA00;
	const uint32_t color_loading = 0x00AA00;
	const uint32_t color_ready = 0x00FFAA;
	const uint32_t color_failed = 0xFF0000;

	TracyCFiberEnter("Async Status");
	if (active_state_ctx.id != 0) {
		TracyCZoneEnd(active_state_ctx);
		active_state_ctx.id = 0;
	}

	switch (new_state) {
		case ASYNC_IDLE: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async IDLE", __func__, TracyFile,
			              (uint32_t)__LINE__, color_idle};
			active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_PENDING: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async PENDING", __func__, TracyFile,
			              (uint32_t)__LINE__, color_pending};
			active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_LOADING: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async LOADING", __func__, TracyFile,
			              (uint32_t)__LINE__, color_loading};
			active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_READY: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async READY", __func__, TracyFile,
			              (uint32_t)__LINE__, color_ready};
			active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
		case ASYNC_FAILED: {
			static const struct ___tracy_source_location_data
			    srcloc = {"Async FAILED", __func__, TracyFile,
			              (uint32_t)__LINE__, color_failed};
			active_state_ctx =
			    ___tracy_emit_zone_begin(&srcloc, active);
			break;
		}
	}
	TracyCFiberLeave;
}

static void cleanup_tracy_states(void)
{
	if (active_state_ctx.id != 0) {
		TracyCFiberEnter("Async Status");
		TracyCZoneEnd(active_state_ctx);
		active_state_ctx.id = 0;
		TracyCFiberLeave;
	}
}
#else
#define transition_tracy_state(s) ((void)0)
#define cleanup_tracy_states() ((void)0)
#endif

static void* async_worker_func(void* arg)
{
	(void)arg; /* Unused */

#ifdef TRACY_ENABLE
	TracyCSetThreadName("Async Loader");
#endif

	pthread_mutex_lock(&request_mutex);
	while (running) {
		while (running && !has_pending_work) {
#ifdef TRACY_ENABLE
			TracyCMessageL("Waiting for work...");
#endif
			pthread_cond_wait(&request_cond, &request_mutex);
		}

		if (!running) {
			break;
		}

		char path_to_load[ASYNC_MAX_PATH];
		bool work_available = false;

		/* 1. Check for work */
		if (current_request.state == ASYNC_PENDING) {
			(void)safe_snprintf(path_to_load, sizeof(path_to_load),
			                    "%s", current_request.path);
			current_request.state = ASYNC_LOADING;
			transition_tracy_state(ASYNC_LOADING);

#ifdef TRACY_ENABLE
			double now = perf_timer_elapsed_ms(&loader_sys_timer);
			double queue_time =
			    now - current_request.submission_time;
			const size_t tracy_msg_size = 128;
			char msg[tracy_msg_size];
			(void)safe_snprintf(msg, sizeof(msg),
			                    "Queuing delay: %.2f ms",
			                    queue_time);
			TracyCMessage(msg, strlen(msg));
#endif
			work_available = true;
		}

		/* Unlock during heavy I/O */
		pthread_mutex_unlock(&request_mutex);

		/* 2. Process work (Disk I/O + Decompression) */
		if (work_available) {
#ifdef TRACY_ENABLE
			TracyCZoneN(work_ctx, "I/O & Docoding", 1);
			TracyCMessage(path_to_load, strlen(path_to_load));
#endif
			int width = 0;
			int height = 0;
			int channels = 0;

			/* Heavy operation triggered here */
			PerfTimer disk_timer;
			perf_timer_start(&disk_timer);
			float* data = texture_load_pixels(path_to_load, &width,
			                                  &height, &channels);
			double load_ms = perf_timer_elapsed_ms(&disk_timer);
#ifdef TRACY_ENABLE
			TracyCZoneEnd(work_ctx);
#endif

			pthread_mutex_lock(&request_mutex);
			if (data) {
				current_request.data = data;
				current_request.width = width;
				current_request.height = height;
				current_request.channels = channels;
				current_request.state = ASYNC_READY;
				transition_tracy_state(ASYNC_READY);
				LOG_INFO("suckless-ogl.async",
				         "Finished loading: %s (%.2f ms)",
				         path_to_load, load_ms);
			} else {
				current_request.state = ASYNC_FAILED;
				transition_tracy_state(ASYNC_FAILED);
				LOG_ERROR("suckless-ogl.async",
				          "Failed loading: %s", path_to_load);
			}
			has_pending_work = false;
			/* Loop continues, mutex is locked */
		} else {
			/* No work found (shouldn't happen if has_pending_work
			   was true, but re-lock to wait) */
			pthread_mutex_lock(&request_mutex);
		}
	}
	pthread_mutex_unlock(&request_mutex);
	return NULL;
}

void async_loader_init(void)
{
	(void)safe_memset(&current_request, sizeof(AsyncRequest), 0,
	                  sizeof(AsyncRequest));
	current_request.state = ASYNC_IDLE;

	if (pthread_mutex_init(&request_mutex, NULL) != 0) {
		LOG_ERROR("suckless-ogl.async", "Mutex init failed");
		return;
	}

	if (pthread_cond_init(&request_cond, NULL) != 0) {
		LOG_ERROR("suckless-ogl.async", "Cond init failed");
		pthread_mutex_destroy(&request_mutex);
		return;
	}

	running = true;
	perf_timer_start(&loader_sys_timer);
	transition_tracy_state(ASYNC_IDLE);

	if (pthread_create(&worker_thread, NULL, async_worker_func, NULL) !=
	    0) {
		LOG_ERROR("suckless-ogl.async", "Thread creation failed");
		running = false;
		pthread_cond_destroy(&request_cond);
		pthread_mutex_destroy(&request_mutex);
	} else {
		LOG_INFO("suckless-ogl.async", "Async loader initialized.");
	}
}

void async_loader_shutdown(void)
{
	if (!running) {
		return;
	}

	pthread_mutex_lock(&request_mutex);
	running = false;
	pthread_cond_broadcast(&request_cond);
	pthread_mutex_unlock(&request_mutex);

	pthread_join(worker_thread, NULL);

	/* Cleanup any pending request data that wasn't consumed */
	if (current_request.data) {
		stbi_image_free(current_request.data);
		current_request.data = NULL;
	}

	pthread_cond_destroy(&request_cond);
	pthread_mutex_destroy(&request_mutex);
	cleanup_tracy_states();
	LOG_INFO("suckless-ogl.async", "Async loader shutdown.");
}

bool async_loader_request(const char* path)
{
#ifdef TRACY_ENABLE
	TracyCZoneN(req_ctx, "async_loader_request", 1);
	TracyCMessage(path, path ? strlen(path) : 0);
#endif
	if (!path) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(req_ctx);
#endif
		return false;
	}

	bool accepted = false;
	pthread_mutex_lock(&request_mutex);

	/* Only accept if idle or failed (retry) */
	if (current_request.state == ASYNC_IDLE ||
	    current_request.state == ASYNC_FAILED ||
	    current_request.state == ASYNC_READY) {
		/* Cleanup previous result if it wasn't consumed */
		if (current_request.data) {
			stbi_image_free(current_request.data);
			current_request.data = NULL;
		}

		if (!safe_snprintf(current_request.path,
		                   sizeof(current_request.path), "%s", path)) {
			LOG_ERROR("suckless-ogl.async", "Path too long: %s",
			          path);
			current_request.state = ASYNC_IDLE;
			transition_tracy_state(ASYNC_IDLE);
		} else {
			current_request.state = ASYNC_PENDING;
			current_request.submission_time =
			    perf_timer_elapsed_ms(&loader_sys_timer);
			transition_tracy_state(ASYNC_PENDING);
			has_pending_work = true;
			pthread_cond_signal(&request_cond);
			accepted = true;
		}
	}

	pthread_mutex_unlock(&request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(req_ctx);
#endif
	return accepted;
}

bool async_loader_poll(AsyncRequest* out_req)
{
#ifdef TRACY_ENABLE
	TracyCZoneN(poll_ctx, "async_loader_poll", 1);
#endif
	if (!out_req) {
#ifdef TRACY_ENABLE
		TracyCZoneEnd(poll_ctx);
#endif
		return false;
	}

	bool result = false;
	pthread_mutex_lock(&request_mutex);

	if (current_request.state == ASYNC_READY) {
		/* Copy result to caller */
		*out_req = current_request;

		/* Clear internal slot */
		current_request.state = ASYNC_IDLE;
		transition_tracy_state(ASYNC_IDLE);
		current_request.data = NULL; /* Ownership transferred */
		result = true;
	} else if (current_request.state == ASYNC_FAILED) {
		/* Failed loading, just reset */
		LOG_ERROR("suckless-ogl.async", "Async load failed for: %s",
		          current_request.path);
		current_request.state = ASYNC_IDLE;
		transition_tracy_state(ASYNC_IDLE);
	}

	pthread_mutex_unlock(&request_mutex);
#ifdef TRACY_ENABLE
	TracyCZoneEnd(poll_ctx);
#endif
	return result;
}
