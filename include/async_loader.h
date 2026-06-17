/**
 * @file async_loader.h
 * @brief Threaded asynchronous file loader engine (Format-agnostic).
 */

#ifndef ASYNC_LOADER_H
#define ASYNC_LOADER_H

#include "asset_manager.h"
#include "gl_common.h"
#include <stdbool.h>
#include <stdint.h>

#define ASYNC_MAX_PATH 256

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
 * @brief Données de la requête asynchrone transférées entre les threads.
 * @note Totalement indépendante des formats d'images concrets.
 */
typedef struct AsyncRequest {
	char path[ASYNC_MAX_PATH];
	AssetType resource_type;

	void* backend_data; /**< Payload opaque géré exclusivement par le module
	                       de format. */
	void* pbo_mapped_ptr; /**< Pointeur vers la mémoire PBO mappée par le
	                         thread principal. */
	GLuint pbo_id;        /**< ID du Pixel Buffer Object OpenGL cible. */
	size_t required_pbo_size; /**< Taille totale requise pour le transfert
	                             en VRAM. */

	int width;
	int height;
	int channels;

	bool is_compressed;
	uint32_t gl_internal_format;
	uint32_t gl_format;
	uint32_t gl_type;

	double submission_time;
	volatile AsyncState state;
} AsyncRequest;

typedef struct AsyncLoader AsyncLoader;
struct TracyManager;

/**
 * @brief Initialize the asynchronous loader subsystem.
 *
 * Allocates resources, initializes synchronization primitives (mutex, condvar),
 * and spawns the background worker thread.
 *
 * @param mgr Tracy profiler manager instance.
 * @return A new AsyncLoader instance on success, NULL on failure.
 *
 * Concurrency: Thread-safe. Lock initialized inside.
 * Initial state: ASYNC_IDLE.
 */
AsyncLoader* async_loader_create(struct TracyManager* mgr);

/**
 * @brief Tear down the asynchronous loader subsystem.
 *
 * Signals the worker thread to exit, waits for its completion (joins it),
 * releases any pending resources/requests, and destroys synchronization
 * primitives.
 *
 * @param loader Loader instance to destroy. Can be NULL (no-op).
 *
 * Concurrency: Safe to call from the main thread during cleanup.
 * Post-condition: Mutex/condvar are destroyed and memory is freed.
 */
void async_loader_destroy(AsyncLoader* loader);

/**
 * @brief Request a new asset to be loaded asynchronously.
 *
 * Accepts a loading task if the loader is idle or finished with the previous
 * one.
 *
 * @param loader Loader instance.
 * @param asset Path and format metadata of the asset.
 * @return true if the request was accepted, false if the loader was busy or
 * invalid.
 *
 * State transitions:
 * - Expected Input State: ASYNC_IDLE, ASYNC_FAILED, or ASYNC_READY.
 * - On success: Transits to ASYNC_PENDING, signals the worker thread.
 * - On failure: State remains unchanged (loader is currently busy:
 * ASYNC_LOADING, ASYNC_WAITING_FOR_PBO, ASYNC_CONVERTING).
 *
 * Concurrency: Thread-safe. Acquires request_mutex.
 */
bool async_loader_request(AsyncLoader* loader, const AssetHandle* asset);

/**
 * @brief Poll the status of the current request.
 *
 * Checks if the request is ready for GPU upload, waiting for PBO allocation,
 * or has failed. This is meant to be called periodically from the main thread.
 *
 * @param loader Loader instance.
 * @param out_req Destination structure to copy the request snapshot if
 * ready/waiting.
 * @return true if a state transition needs handling (READY, WAITING_FOR_PBO,
 * FAILED), false otherwise.
 *
 * State transitions:
 * - Expected Input State: Any state.
 * - If current state is ASYNC_READY: Copies request, resets state to
 * ASYNC_IDLE, returns true.
 * - If current state is ASYNC_FAILED: Copies request, resets state to
 * ASYNC_IDLE, returns true.
 * - If current state is ASYNC_WAITING_FOR_PBO: Copies request, state remains
 * ASYNC_WAITING_FOR_PBO, returns true.
 * - For any other state: Returns false (no action needed yet).
 *
 * Concurrency: Thread-safe. Acquires request_mutex.
 */
bool async_loader_poll(AsyncLoader* loader, AsyncRequest* out_req);

/**
 * @brief Provide the mapped memory address and GL buffer ID for PBO writing.
 *
 * Called by the main thread after detecting ASYNC_WAITING_FOR_PBO during poll.
 * Maps the CPU pointer to the PBO and allows the worker to perform the
 * conversion.
 *
 * @param loader Loader instance.
 * @param mapped_ptr Pointer to the mapped buffer memory (CPU visible).
 * @param pbo_id OpenGL buffer object ID (or 0 for CPU-only malloc fallback).
 *
 * State transitions:
 * - Expected Input State: ASYNC_WAITING_FOR_PBO.
 * - On success: Transits to ASYNC_CONVERTING, signals the worker thread to
 * resume.
 * - If called in any other state: Ignored (no-op).
 *
 * Concurrency: Thread-safe. Acquires request_mutex.
 */
void async_loader_provide_pbo(AsyncLoader* loader, void* mapped_ptr,
                              GLuint pbo_id);

/**
 * @brief Cancel a loading request waiting for PBO input.
 *
 * Aborts the current request if it is still waiting for the main thread to
 * provide a PBO.
 *
 * @param loader Loader instance.
 *
 * State transitions:
 * - Expected Input State: ASYNC_WAITING_FOR_PBO.
 * - On success: Transits to ASYNC_FAILED, signals the worker thread to cleanup.
 * - If called in any other state (e.g. ASYNC_CONVERTING): Ignored to prevent
 * data corruption.
 *
 * Concurrency: Thread-safe. Acquires request_mutex.
 */
void async_loader_cancel(AsyncLoader* loader);

#include "app_subsystem.h"
int async_loader_subsys_init(struct App* app);
void async_loader_subsys_cleanup(struct App* app);
#define APP_ASYNC_LOADER_DESCRIPTOR \
	{"async_loader", async_loader_subsys_init, async_loader_subsys_cleanup}

#endif /* ASYNC_LOADER_H */
