#ifndef SCENE_CONFIG_H
#define SCENE_CONFIG_H

#include <stdbool.h>

/**
 * @file scene_config.h
 * @brief Render configuration flags extracted from Scene.
 */

/**
 * @enum SortingMode
 * @brief Sorting algorithms for transparent billboards.
 */
typedef enum {
	SORTING_MODE_CPU_QSORT = 0,
	SORTING_MODE_CPU_RADIX,
	SORTING_MODE_GPU_BITONIC,
	SORTING_MODE_COUNT /**< Sentinel — must remain last. */
} SortingMode;

/**
 * @enum GIMode
 * @brief Global Illumination sampling methods.
 */
typedef enum {
	GI_MODE_OFF = 0,
	GI_MODE_3D_TEX,
	GI_MODE_SSBO,
	GI_MODE_COUNT
} GIMode;

/**
 * @enum AAMode
 * @brief Specular Anti-Aliasing modes.
 */
typedef enum {
	AA_MODE_SCREEN_SPACE = 0,
	AA_MODE_CURVATURE,
	AA_MODE_COUNT
} AAMode;

/**
 * @struct SceneConfig
 * @brief Render configuration flags and toggles.
 */
typedef struct SceneConfig {
	int wireframe;            /**< OpenGL wireframe mode toggle. */
	int billboard_mode;       /**< Toggle for billboard rendering path. */
	SortingMode sorting_mode; /**< Selected sorting algorithm. */
	int pbr_debug_mode;       /**< Swap to wireframe/normal/roughness */
	bool show_envmap;         /**< Draw skybox toggle. */
	float env_lod;            /**< Skybox blurriness. */
	int subdivisions;         /**< LOD of the shared icosphere. */
	GIMode gi_mode;           /**< Selected GI sampling method. */
	bool show_probe_grid;     /**< Debug visualization of probes. */
	int specular_aa_enabled;  /**< Screen-Space Specular Anti-Aliasing. */
	AAMode aa_mode;           /**< AA Mode: Screen-space or Curvature. */
} SceneConfig;

#endif /* SCENE_CONFIG_H */
