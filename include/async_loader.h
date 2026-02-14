/**
 * @file async_loader.h
 * @brief Threaded asynchronous file loader for heavy assets (HDR, textures).
 *
 * This module manages a background worker thread that handles I/O and
 * image decoding, preventing main-thread stalls during asset transitions.
 */

#ifndef ASYNC_LOADER_H
#define ASYNC_LOADER_H

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
	ASYNC_LOADING, /**< Worker is currently reading or decoding the file. */
	ASYNC_READY, /**< Data is available and ready to be uploaded to GPU. */
	ASYNC_FAILED /**< Error encountered during load (missing file, etc). */
} AsyncState;

/**
 * @struct AsyncRequest
 * @brief Container for asynchronous load results and metadata.
 */
typedef struct AsyncRequest {
	char path[ASYNC_MAX_PATH]; /**< Absolute path to the source file. */
	float* data;  /**< Raw pixel data (must be freed by caller). */
	int width;    /**< Image width in pixels. */
	int height;   /**< Image height in pixels. */
	int channels; /**< Number of color channels (e.g., 3 for RGB). */
	double submission_time; /**< Time when request was submitted. */
	volatile AsyncState
	    state; /**< Current state (atomic/volatile for thread-safety). */
} AsyncRequest;

/**
 * @brief Spawns the background worker thread.
 * @note Must be called once during application startup.
 */
void async_loader_init(void);

/**
 * @brief Signals the worker thread to exit and joins it.
 * @note Clean up all pending requests and free the internal queue.
 */
void async_loader_shutdown(void);

/**
 * @brief Submits a new file path for background loading.
 * @param path The absolute path to the HDR/texture file.
 * @return true if the request was successfully queued, false if queue is full.
 */
bool async_loader_request(const char* path);

/**
 * @brief Polls the loader for any completed requests.
 *
 * This should be called from the main (OpenGL) thread once per frame.
 * @param[out] out_request Pointer to store the successfully loaded data.
 * @return true if data was retrieved, false otherwise.
 */
bool async_loader_poll(AsyncRequest* out_request);

#endif /* ASYNC_LOADER_H */
