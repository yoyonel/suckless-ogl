/**
 * @file app_env.h
 * @brief Environment and IBL management module.
 *
 * This module handles HDR texture file scanning, environment map loading,
 * and manages the state machine for progressive Image-Based Lighting (IBL)
 * updates.
 *
 */

#ifndef APP_ENV_H
#define APP_ENV_H

#include "app_settings.h"
#include "async_loader.h"
#include "gl_common.h"

typedef struct App App;

/**
 * @enum EnvLoadingStep
 * @brief Steps for the multi-frame HDR environment loading process.
 */
typedef enum {
	ENV_LOAD_IDLE = 0,
	ENV_LOAD_UPLOAD,  /**< Step 1: Upload texture to GPU. */
	ENV_LOAD_MIPMAPS, /**< Step 2: Generate Mipmaps. */
	ENV_LOAD_IBL      /**< Step 3: Start IBL Coordinator. */
} EnvLoadingStep;

/**
 * @struct EnvManager
 * @brief Encapsulates state for environment loading, transitions, and IBL.
 */
typedef struct EnvManager {
	int env_map_loading; /**< Async lock for HDR loading. */
	EnvLoadingStep
	    env_map_loading_step; /**< Multi-frame loading step counter. */
	AsyncRequest
	    current_env_req; /**< Currently processing async request. */
	TransitionState transition_state;
	float transition_alpha;
	float transition_duration;
	int is_first_load;
	int env_transition_mode; /**< EnvTransitionMode. */
	GLuint pending_env_tex;  /**< Texture being assembled before IBL. */
} EnvManager;

/**
 * @brief Scans the assets directory for HDR environment maps.
 *
 * Populates the list of available files in the application state.
 * @param app Pointer to the application state.
 * @note Fills app->hdr_files and app->hdr_count.
 */
void app_scan_hdr_files(App* app);

/**
 * @brief Starts an asynchronous load of an environment map.
 * @param app Pointer to the application state.
 * @param filename Basename of the HDR file to load.
 * @return 1 on success, 0 on failure.
 */
int app_load_env_map(App* app, const char* filename);

/**
 * @brief Processes the current state of the IBL generation state machine.
 *
 * Should be called once per frame from the main loop. Handles progressive
 * generation of irradiance and pre-filtered specular maps.
 * @param app Pointer to the application state.
 */
void app_process_ibl_state_machine(App* app);

/**
 * @brief Processes the multi-frame HDR environment loading state machine.
 * @param app Pointer to the application state.
 */
void app_process_env_map_loading_step(App* app);

/**
 * @brief Updates the environment transition.
 * @param app Pointer to the application state.
 */
void app_update_transition(App* app);

/**
 * @brief Triggers a new environment transition.
 * @param app Pointer to the application state.
 * @param filename HDR file to load.
 * @return 1 on success.
 */
int app_trigger_env_transition(App* app, const char* filename);

#endif /* APP_ENV_H */
