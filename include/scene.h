#ifndef SCENE_H
#define SCENE_H

#include "app_settings.h"
#include "billboard_rendering.h"
#include "billboard_sorting.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "scene_config.h"
#include "scene_lighting.h"
#include "scene_visuals.h"
#include <cglm/cglm.h>

#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif

typedef struct PostProcess PostProcess;
typedef struct GPUProfiler GPUProfiler;
typedef struct SceneSimulation SceneSimulation;
typedef struct SceneShaders SceneShaders;
typedef struct SceneGPUResources SceneGPUResources;

/**
 * @struct Scene
 * @brief Encapsulates all 3D scene data, geometry, and rendering state.
 */
typedef struct Scene {
	/* --- Geometry & Meshes --- */
	IcosphereGeometry geometry; /**< High-poly sphere mesh data. */
	InstancedGroup
	    instanced_group; /**< Managed buffers for opaque spheres. */
	BillboardGroup billboard_group; /**< Managed buffers for billboards. */
#ifdef USE_SSBO_RENDERING
	SSBOGroup ssbo_group; /**< SSBO rendering context. */
#endif

#ifdef USE_TRANSPARENT_BILLBOARDS
	BillboardSorter billboard_sorter; /**< Sorter for alpha blending. */
	SphereInstance*
	    billboard_instances;      /**< Persistent array for sorting. */
	int billboard_instance_count; /**< Active billboard count. */
#endif

	/* --- Domain Sub-Structs (opaque, heap-allocated) --- */
	SceneSimulation* simulation; /**< N-body simulation sub-system. */
	SceneGPUResources* gpu;      /**< GPU resource handles. */
	SceneShaders* shaders;       /**< Shader pointers + uniform caches. */

	/* --- Domain Sub-Structs (by-value) --- */
	SceneVisuals visuals;   /**< Visual effects sub-system. */
	SceneLighting lighting; /**< IBL, probes, and materials. */
	SceneConfig config;     /**< Render configuration. */

	/* --- HDR Environment --- */
	char** hdr_files;      /**< List of found HDR files in assets. */
	int hdr_count;         /**< Number of available environment maps. */
	int current_hdr_index; /**< Index of active HDR in file list. */

} Scene;

/**
 * @brief Initializes the scene resources.
 * @param scene Pointer to the scene structure.
 * @return 1 on success, 0 on failure.
 */
int scene_init(Scene* scene);

/**
 * @brief Cleans up scene resources.
 * @param scene Pointer to the scene structure.
 */
void scene_cleanup(Scene* scene);

/**
 * @brief Returns a string representation of the AA mode.
 * @param mode The AA mode.
 * @return String representation.
 */
const char* aa_mode_to_string(AAMode mode);

/**
 * @brief Renders the scene.
 * @param scene Pointer to the scene.
 * @param view View matrix.
 * @param proj Projection matrix.
 * @param camera_pos Camera position.
 * @param previous_view_proj Previous frame's ViewProj matrix (for motion blur).
 * @param width Viewport width.
 * @param height Viewport height.
 */
void scene_render(Scene* scene, GPUProfiler* profiler, mat4 view, mat4 proj,
                  vec3 camera_pos, mat4 previous_view_proj, int width,
                  int height);

/**
 * @brief Updates GPU buffers for dynamic geometry (e.g. LOD changes).
 * @param scene Pointer to the scene.
 */
void scene_update_gpu_buffers(Scene* scene);

/**
 * @brief Toggles N-body simulation mode on/off.
 * @param scene Pointer to the scene.
 */
void scene_toggle_nbody(Scene* scene);

/**
 * @brief Advances the N-body simulation by one frame.
 * @param scene Pointer to the scene.
 * @param delta_time Wall-clock time elapsed.
 * @param cam_pos Camera world position (for trail billboard).
 */
void scene_nbody_update(Scene* scene, float delta_time);

#endif /* SCENE_H */
