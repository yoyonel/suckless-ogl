/**
 * @file app_env.h
 * @brief Environment and IBL management module.
 *
 * This module handles HDR texture file scanning, environment map loading,
 * and manages the state machine for progressive Image-Based Lighting (IBL)
 * updates.
 *
 * \dot
 * digraph IBLState {
 *   rankdir=LR;
 *   node [shape=ellipse, style=filled, fillcolor="#f8f8f8",
 * fontname="Helvetica", fontsize=9]; edge [fontname="Helvetica", fontsize=9];
 *
 *   IDLE [shape=doublecircle, fillcolor="#e0e0e0"];
 *   LUMINANCE [label="Luminance\nPass"];
 *   SPEC_INIT [label="Specular\nInit"];
 *   SPEC_MIPS [label="Specular\nMips"];
 *   IRRADIANCE [label="Irradiance\nPass"];
 *   DONE [shape=doublecircle, fillcolor="#ccffcc", label="DONE"];
 *
 *   IDLE -> LUMINANCE [label="New HDR Loaded"];
 *   LUMINANCE -> SPEC_INIT [label="Auto-Exposure"];
 *   SPEC_INIT -> SPEC_MIPS [label="Alloc"];
 *   SPEC_MIPS -> SPEC_MIPS [label="Next Mip/Slice"];
 *   SPEC_MIPS -> IRRADIANCE [label="All Mips"];
 *   IRRADIANCE -> IRRADIANCE [label="Next Slice"];
 *   IRRADIANCE -> DONE [label="Finished"];
 *   DONE -> IDLE [label="Swap & Cleanup"];
 * }
 * \enddot
 */

#ifndef APP_ENV_H
#define APP_ENV_H

typedef struct App App;
typedef struct AsyncRequest AsyncRequest;

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
 * @brief Finalizes the HDR texture upload once the async loader is ready.
 * @param app Pointer to the application state.
 * @param req Pointer to the completed async request.
 * @note Transitions the IBL context into the processing phase.
 */
void app_finalize_environment_load(App* app, AsyncRequest* req);

#endif /* APP_ENV_H */
