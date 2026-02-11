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

static void* async_worker_func(void* arg)
{
	(void)arg; /* Unused */

	pthread_mutex_lock(&request_mutex);
	while (running) {
		while (running && !has_pending_work) {
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
			work_available = true;
		}

		/* Unlock during heavy I/O */
		pthread_mutex_unlock(&request_mutex);

		/* 2. Process work (Disk I/O + Decompression) */
		if (work_available) {
			int width = 0;
			int height = 0;
			int channels = 0;

			/* Heavy operation triggered here */
			PerfTimer disk_timer;
			perf_timer_start(&disk_timer);
			float* data = texture_load_pixels(path_to_load, &width,
			                                  &height, &channels);
			double load_ms = perf_timer_elapsed_ms(&disk_timer);

			pthread_mutex_lock(&request_mutex);
			if (data) {
				current_request.data = data;
				current_request.width = width;
				current_request.height = height;
				current_request.channels = channels;
				current_request.state = ASYNC_READY;
				LOG_INFO("suckless-ogl.async",
				         "Finished loading: %s (%.2f ms)",
				         path_to_load, load_ms);
			} else {
				current_request.state = ASYNC_FAILED;
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
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	(void)memset(&current_request, 0, sizeof(AsyncRequest));
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
	LOG_INFO("suckless-ogl.async", "Async loader shutdown.");
}

bool async_loader_request(const char* path)
{
	if (!path) {
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
		} else {
			current_request.state = ASYNC_PENDING;
			has_pending_work = true;
			pthread_cond_signal(&request_cond);
			accepted = true;
		}
	}

	pthread_mutex_unlock(&request_mutex);
	return accepted;
}

bool async_loader_poll(AsyncRequest* out_req)
{
	if (!out_req) {
		return false;
	}

	bool result = false;
	pthread_mutex_lock(&request_mutex);

	if (current_request.state == ASYNC_READY) {
		/* Copy result to caller */
		*out_req = current_request;

		/* Clear internal slot */
		current_request.state = ASYNC_IDLE;
		current_request.data = NULL; /* Ownership transferred */
		result = true;
	} else if (current_request.state == ASYNC_FAILED) {
		/* Failed loading, just reset */
		LOG_ERROR("suckless-ogl.async", "Async load failed for: %s",
		          current_request.path);
		current_request.state = ASYNC_IDLE;
	}

	pthread_mutex_unlock(&request_mutex);
	return result;
}
