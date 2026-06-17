/**
 * @file env_manager.h
 * @brief Environment and IBL management module.
 *
 * This module handles HDR texture file scanning, environment map loading,
 * and manages the state machine for progressive Image-Based Lighting (IBL)
 * updates.
 */

#ifndef ENV_MANAGER_H
#define ENV_MANAGER_H

#include "app_settings.h"
#include "async_loader.h"
#include "gl_common.h"
#include "ibl_coordinator.h"
#include <stdbool.h>

// Forward declarations
typedef struct Scene Scene;
typedef struct PostProcess PostProcess;

typedef enum {
	EML_STOP = 0,
	EML_UPLOAD_HDR_FROM_PBO,
	EML_MIPMAP,
	EML_START_IBL,
} EnvMapLoadingState;

/**
 * @struct EnvManager
 * @brief Encapsulates state for environment loading, transitions, and IBL.
 */
typedef struct EnvManager {
	bool env_map_loading; /**< Async lock for HDR loading. */
	EnvMapLoadingState
	    env_map_loading_step; /**< Multi-frame loading step counter. */
	AsyncRequest
	    current_env_req; /**< Currently processing async request. */
	TransitionState transition_state;
	float transition_alpha;
	float transition_duration;
	bool is_first_load;
	int env_transition_mode; /**< EnvTransitionMode. */
	GLuint pending_env_tex;  /**< Texture being assembled before IBL. */
} EnvManager;

/**
 * @brief Starts an asynchronous load of an environment map.
 * @param mgr Pointer to the environment manager.
 * @param loader Pointer to the async loader.
 * @param filename Basename of the HDR file to load.
 * @return 1 on success, 0 on failure.
 */
int env_manager_load(EnvManager* mgr, AsyncLoader* loader,
                     const char* filename);

/**
 * @brief Processes the multi-frame HDR environment loading state machine.
 * @param mgr Pointer to the environment manager.
 * @param recycled_hdr_tex Pointer to the recycled HDR texture handle.
 * @param ibl Pointer to the IBL coordinator.
 */
void env_manager_process_loading_step(EnvManager* mgr, GLuint* recycled_hdr_tex,
                                      IBLCoordinator* ibl);

/**
 * @brief Triggers a new environment transition.
 * @param mgr Pointer to the environment manager.
 * @param loader Pointer to the async loader.
 * @param filename HDR file to load.
 * @return 1 on success.
 */
int env_manager_trigger_transition(EnvManager* mgr, AsyncLoader* loader,
                                   const char* filename);

/**
 * @brief Updates the IBL state machine and handles transition events.
 *
 * This function combines the logic of checking IBL progress and updating
 * the transition state (loading -> fade out/in).
 *
 * @param mgr Pointer to the environment manager.
 * @param scene Pointer to the scene (for texture management).
 * @param postproc Pointer to post-process (for exposure).
 * @param frame_count Current frame count.
 * @param width Current window width (for snapshot).
 * @param height Current window height (for snapshot).
 */
void env_manager_update_ibl(EnvManager* mgr, Scene* scene,
                            PostProcess* postproc, uint64_t frame_count,
                            int width, int height);

/**
 * @brief Updates the environment transition animation.
 * @param mgr Pointer to the environment manager.
 * @param scene Pointer to the scene.
 * @param postproc Pointer to post-process.
 * @param delta_time Time elapsed since last frame.
 * @param frame_count Current frame count.
 */
void env_manager_update_transition(EnvManager* mgr, Scene* scene,
                                   PostProcess* postproc, double delta_time,
                                   uint64_t frame_count);

/**
 * @brief Renders the transition overlay (fade/crossfade).
 * @param mgr Pointer to the environment manager.
 * @param scene Pointer to the scene (contains debug shader and quad).
 */
void env_manager_render_overlay(const EnvManager* mgr, const Scene* scene);

/* --- Subsystem descriptor (alloc-only Phase 1) --- */
#include "app_subsystem.h"

int env_mgr_subsys_init(struct App* app);
void env_mgr_subsys_cleanup(struct App* app);

#define APP_ENV_MGR_DESCRIPTOR \
	{"env_mgr", env_mgr_subsys_init, env_mgr_subsys_cleanup}

#endif /* ENV_MANAGER_H */
