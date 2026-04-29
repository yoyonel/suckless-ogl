#ifndef TRACY_MANAGER_H
#define TRACY_MANAGER_H

#include "async_loader.h"

#ifdef TRACY_ENABLE
#include "tracy/TracyC.h"
#include "tracy_gpu.h"
#include <pthread.h>
#endif

typedef struct App App;

/**
 * @struct TracyManager
 * @brief Handles Tracy-specific instrumentation and asynchronous GPU
 * screenshots.
 */
typedef struct TracyManager {
#ifdef TRACY_ENABLE
#define TRACY_PBO_COUNT 4
	GLuint screenshot_pbo[TRACY_PBO_COUNT];
	GLsync screenshot_sync[TRACY_PBO_COUNT];
	GLuint screenshot_fbo;
	GLuint screenshot_tex;
	int screenshot_pbo_idx;

	/* --- Encapsulated State --- */
	TracyCZoneCtx active_state_ctx;
	pthread_mutex_t transition_mutex;
#endif
} TracyManager;

/**
 * @brief Global Tracy initialization (cJSON hooks, etc).
 * Must be called as early as possible in main().
 */
void tracy_manager_init_global(void);

/**
 * @brief Initialize a TracyManager instance.
 */
void tracy_manager_init(TracyManager* mgr, int width, int height);

/**
 * @brief Cleanup TracyManager resources.
 */
void tracy_manager_cleanup(TracyManager* mgr);

/**
 * @brief Asynchronously capture and send screenshots to Tracy.
 * Should be called once per frame.
 */
void tracy_manager_update_screenshots(TracyManager* mgr, App* app);

/**
 * @brief Log a state transition for the AsyncLoader to Tracy.
 */
void tracy_manager_async_transition(TracyManager* mgr, AsyncState new_state);

/**
 * @brief End any active async tracking zone in Tracy.
 */
void tracy_manager_async_end(TracyManager* mgr);

#endif  // TRACY_MANAGER_H
