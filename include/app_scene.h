/**
 * @file app_scene.h
 * @brief Scene rendering module for billboards and instanced geometry.
 *
 * This module handles the initialization, updating, and rendering of scene
 * objects, supporting both traditional instancing and billboard-based
 * transparency.
 */

#ifndef APP_SCENE_H
#define APP_SCENE_H

typedef struct App App;
#include <cglm/types.h>

/**
 * @brief Initializes instancing resources (buffers, sorting structures).
 * @param app Pointer to the application state.
 */
void app_init_instancing(App* app);

/**
 * @brief Updates the instancing mode based on current application settings.
 * @param app Pointer to the application state.
 */
void app_update_instancing_mode(App* app);

/**
 * @brief Renders the scene as spherical billboards.
 *
 * Used primarily for transparent/alpha-blended spheres.
 * @param app Pointer to the application state.
 * @param view Current view matrix.
 * @param proj Current projection matrix.
 * @param camera_pos Current camera world position.
 */
void app_render_billboards(App* app, mat4 view, mat4 proj, vec3 camera_pos);

/**
 * @brief Renders the scene using hardware instancing.
 *
 * Used for opaque PBR spheres.
 * @param app Pointer to the application state.
 * @param view Current view matrix.
 * @param proj Current projection matrix.
 * @param camera_pos Current camera world position.
 */
void app_render_instanced(App* app, mat4 view, mat4 proj, vec3 camera_pos);

/**
 * @brief Synchronizes vertex geometry with the GPU.
 *
 * Updates vertex, normal, and element buffers for the core sphere geometry.
 * @param app Pointer to the application state.
 */
void app_update_gpu_buffers(App* app);

#ifdef USE_SSBO_RENDERING
/**
 * @brief Initializes Shader Storage Buffer Object (SSBO) for large-scale
 * instancing.
 * @param app Pointer to the application state.
 */
void app_init_ssbo(App* app);
#endif

#endif /* APP_SCENE_H */
