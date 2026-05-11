/**
 * @file async_loader.h
 * @brief Threaded asynchronous file loader for heavy assets (HDR, textures).
 *
 * This module manages a background worker thread that handles I/O and
 * image decoding, preventing main-thread stalls during asset transitions.
 */

#ifndef ASYNC_LOADER_H
#define ASYNC_LOADER_H

#include "gl_common.h"
#include <stdbool.h>

/** @brief Maximum path length for an asynchronous load request. */
#define ASYNC_MAX_PATH 256

/**
 * @enum AsyncState
 * @brief Lifecycle states for an individual asynchronous request.
 */
typedef enum {
	ASYNC_IDLE = 0, /**< No active request. */
	ASYNC_PENDING,  /**< Request submitted but not yet picked up by worker.
	                 */
	ASYNC_LOADING,  /**< Worker is loading/decompressing file */
	ASYNC_WAITING_FOR_PBO, /**< Worker waiting for PBO from main thread */
	ASYNC_CONVERTING,      /**< Worker converting floats to mapped PBO */
	ASYNC_READY,           /**< Data is ready in PBO (or half_data if
	                          fallback) */
	ASYNC_FAILED           /**< Loading failed */
} AsyncState;

/**
 * @struct AsyncRequest
 * @brief Container for asynchronous load results and metadata.
 */
#include <stdint.h>

typedef struct AsyncRequest {
	char path[ASYNC_MAX_PATH]; /**< Absolute path to the source file. */
	/* --- Internal Data for Async Ops --- */
	float* float_data;   /* For loading stage */
	uint16_t* half_data; /* For legacy upload (if no PBO) */
	void* pbo_mapped_ptr;
	GLuint pbo_id; /* ID of the PBO used for this request */
	int width;     /**< Image width in pixels. */
	int height;    /**< Image height in pixels. */
	int channels;  /**< Number of color channels (e.g., 3 for RGB). */
	double submission_time; /**< Time when request was submitted. */
	volatile AsyncState
	    state; /**< Current state (atomic/volatile for thread-safety). */
} AsyncRequest;

/**
 * @struct AsyncLoader
 * @brief Opaque handle to the asynchronous loader context.
 */
typedef struct AsyncLoader AsyncLoader;

struct TracyManager;

/**
 * @brief Creates and initializes a new async loader instance.
 * @param mgr The Tracy instrumentation manager.
 * @return Pointer to the new loader, or NULL on failure.
 */
AsyncLoader* async_loader_create(struct TracyManager* mgr);

/**
 * @brief Destroys the async loader and frees resources.
 * @param loader The loader instance to destroy.
 */
void async_loader_destroy(AsyncLoader* loader);

/**
 * @brief Submits a new file path for background loading.
 * @param loader The loader instance.
 * @param path The absolute path to the HDR/texture file.
 * @return true if the request was successfully queued, false if queue is full.
 */
bool async_loader_request(AsyncLoader* loader, const char* path);

/**
 * @brief Polls the loader for any completed requests.
 *
 * This should be called from the main (OpenGL) thread once per frame.
 * @param loader The loader instance.
 * @param[out] out_req Pointer to store the successfully loaded data.
 * @return true if data was retrieved, false otherwise.
 */
bool async_loader_poll(AsyncLoader* loader, AsyncRequest* out_req);

/**
 * @brief Provides a mapped PBO pointer to the async loader for conversion.
 *
 * Call this when async_loader_poll returns a request in
 * ASYNC_WAITING_FOR_PBO state.
 * @param loader The loader instance.
 * @param mapped_ptr Pointer to the mapped PBO memory.
 * @param pbo_id ID of the PBO being used.
 */
void async_loader_provide_pbo(AsyncLoader* loader, void* mapped_ptr,
                              GLuint pbo_id);

/**
 * @brief Cancels the current request if it is waiting for a PBO.
 *
 * Use this if the main thread fails to map a PBO and cannot proceed.
 * @param loader The loader instance.
 */
void async_loader_cancel(AsyncLoader* loader);

#include "app_subsystem.h"
int async_loader_subsys_init(struct App* app);
void async_loader_subsys_cleanup(struct App* app);
#define APP_ASYNC_LOADER_DESCRIPTOR \
	{"async_loader", async_loader_subsys_init, async_loader_subsys_cleanup}

#endif /* ASYNC_LOADER_H */
