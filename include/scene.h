#ifndef SCENE_H
#define SCENE_H

#include "app_settings.h"
#include "billboard_rendering.h"
#include "gl_common.h"
#include "ibl_coordinator.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "light_probes.h"
#include "material.h"
#include "scene_renderer.h"
#include "shader.h"
#include "skybox.h"
#include "sphere_sorting.h"
#include "ssbo_rendering.h"
#include <cglm/cglm.h>

typedef struct PostProcess PostProcess;

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
 * @struct InstancedUniforms
 * @brief Cached uniform locations for PBR instanced rendering.
 */
typedef struct {
	GLint irradiance_map;        /**< Location of 'irradianceMap' */
	GLint prefilter_map;         /**< Location of 'prefilterMap' */
	GLint brdf_lut;              /**< Location of 'brdfLUT' */
	GLint debug_mode;            /**< Location of 'debugMode' */
	GLint cam_pos;               /**< Location of 'camPos' */
	GLint projection;            /**< Location of 'projection' */
	GLint view;                  /**< Location of 'view' */
	GLint previous_view_proj;    /**< Location of 'previousViewProj' */
	GLint probe_grid_min;        /**< Location of 'u_ProbeGridMin' */
	GLint probe_grid_max;        /**< Location of 'u_ProbeGridMax' */
	GLint probe_grid_dim;        /**< Location of 'u_ProbeGridDim' */
	GLint gi_mode;               /**< Location of 'u_GIMode' */
	GLint u_specular_aa_enabled; /**< Location of 'u_specularAAEnabled' */
	GLint u_aa_mode;             /**< Location of 'u_aaMode' */
	GLint
	    sh_textures[SH_TEXTURE_COUNT]; /**< Locations of 'u_SHTexture0-6' */
} InstancedUniforms;

/**
 * @struct DebugUniforms
 * @brief Cached uniform locations for debug line rendering.
 */
typedef struct {
	GLint projection;         /**< Location of 'projection' */
	GLint view;               /**< Location of 'view' */
	GLint u_stippled;         /**< Location of 'u_stippled' */
	GLint u_billboard_mode;   /**< Location of 'u_billboardMode' */
	GLint u_use_instance_col; /**< Location of 'u_useInstanceColor' */
	GLint u_color;            /**< Location of 'u_color' */
} DebugUniforms;

/**
 * @struct BillboardUniforms
 * @brief Cached uniform locations for billboard rendering.
 */
typedef struct {
	GLint irradiance_map;        /**< Location of 'irradianceMap' */
	GLint prefilter_map;         /**< Location of 'prefilterMap' */
	GLint brdf_lut;              /**< Location of 'brdfLUT' */
	GLint debug_mode;            /**< Location of 'debugMode' */
	GLint cam_pos;               /**< Location of 'camPos' */
	GLint projection;            /**< Location of 'projection' */
	GLint view;                  /**< Location of 'view' */
	GLint previous_view_proj;    /**< Location of 'previousViewProj' */
	GLint u_screen_size;         /**< Location of 'u_screenSize' */
	GLint probe_grid_min;        /**< Location of 'u_ProbeGridMin' */
	GLint probe_grid_max;        /**< Location of 'u_ProbeGridMax' */
	GLint probe_grid_dim;        /**< Location of 'u_ProbeGridDim' */
	GLint gi_mode;               /**< Location of 'u_GIMode' */
	GLint grid_to_idx_scale;     /**< Location of 'u_GridToIdxScale' */
	GLint u_specular_aa_enabled; /**< Location of 'u_specularAAEnabled' */
	GLint u_aa_mode;             /**< Location of 'u_aaMode' */
	GLint
	    sh_textures[SH_TEXTURE_COUNT]; /**< Locations of 'u_SHTexture0-6' */
} BillboardUniforms;

typedef struct Scene Scene;

/**
 * @struct Scene
 * @brief Encapsulates all 3D scene data, geometry, and rendering state.
 */
typedef struct Scene {
	/* --- Geometry & Meshes --- */
	IcosphereGeometry geometry;     /**< High-poly sphere mesh data. */
	InstancedGroup instanced_group; /**< Buffers for opaque spheres. */
	BillboardGroup billboard_group; /**< Buffers for billboards. */
	SSBOGroup ssbo_group;           /**< SSBO rendering context. */

	SphereSorter sphere_sorter;       /**< Sorter for alpha blending. */
	SphereInstance* sphere_instances; /**< Persistent array for sorting. */
	int sphere_instance_count;        /**< Active sphere count. */

	const SceneRenderer* renderer; /**< Active rendering strategy. */

	Skybox skybox; /**< Environment renderer (Shaders owned by Scene). */
	MaterialLib* material_lib; /**< Loaded material presets. */
	char** hdr_files;          /**< List of found HDR files in assets. */
	int hdr_count;             /**< Number of available environment maps. */
	int current_hdr_index;     /**< Index of active HDR in file list. */
	IBLCoordinator ibl_coord;  /**< IBL state machine (Compute shaders owned
	                              by Scene). */
	LightProbeGrid probe_grid; /**< Global Illumination spatial grid. */

	/* --- Shaders --- */
	Shader* pbr_instanced_shader; /**< Shared PBR shader for opaque geo. */
	Shader*
	    pbr_billboard_shader;  /**< Shader for volumetric/alpha spheres. */
	Shader* pbr_ssbo_shader;   /**< Optimized SSBO shader. */
	Shader* debug_shader;      /**< Generic debug/visualization shader. */
	Shader* debug_line_shader; /**< Shader for wireframe lines. */
	Shader* skybox_shader;     /**< Skybox shader wrapper. */

	/* --- GPU Resources --- */
	GLuint sphere_vao;           /**< Shared geometry VAO. */
	GLuint sphere_vbo;           /**< Shared vertex buffer. */
	GLuint sphere_nbo;           /**< Shared normal buffer. */
	GLuint sphere_ebo;           /**< Shared index buffer. */
	GLuint quad_vbo;             /**< Shared full-screen quad (FSQ). */
	GLuint wire_cube_vbo;        /**< Shared wireframe cube. */
	GLuint wire_quad_vbo;        /**< Shared wireframe quad. */
	GLuint hdr_texture;          /**< Active HDR cubemap. */
	GLuint recycled_hdr_tex;     /**< Recycled texture for next load. */
	GLuint spec_prefiltered_tex; /**< Active Specular map. */
	GLuint irradiance_tex;       /**< Active Irradiance map. */
	GLuint brdf_lut_tex;         /**< Shared BRDF lookup table. */
	GLuint empty_vao;            /**< Vertex-less drawing VAO. */
	GLuint shader_spmap;         /**< Internal IBL specular shader. */
	GLuint shader_irmap;         /**< Internal IBL irradiance shader. */
	GLuint shader_lum_pass1;     /**< Luminance downsample pass. */
	GLuint shader_lum_pass2;     /**< Mean luminance compute pass. */
	GLuint dummy_black_tex;      /**< Safe fallback (0,0,0,1). */
	GLuint dummy_white_tex;      /**< Safe fallback (1,1,1,1). */
	GLuint lum_ssbo[2]; /**< Double-buffered storage for luminance. */
	GLuint transition_snapshot_tex; /**< For crossfade mode. */

	/* --- Render Configuration --- */
	int wireframe;            /**< OpenGL wireframe mode toggle. */
	int billboard_mode;       /**< Toggle for billboard rendering path. */
	SortingMode sorting_mode; /**< Selected sorting algorithm. */
	int pbr_debug_mode;       /**< Swap to wireframe/normal/roughness */
	int show_envmap;          /**< Draw skybox toggle. */
	float env_lod;            /**< Skybox blurriness. */
	int subdivisions;         /**< LOD of the shared icosphere. */
	GIMode gi_mode;           /**< Selected GI sampling method. */
	int show_probe_grid;      /**< Debug visualization of probes. */
	int specular_aa_enabled;  /**< Screen-Space Specular Anti-Aliasing. */
	AAMode aa_mode;           /**< AA Mode: Screen-space or Curvature. */

	/* --- Uniform Caches --- */
	BillboardUniforms billboard_uniforms; /**< Cached locations. */
	InstancedUniforms instanced_uniforms; /**< Cached locations. */
	DebugUniforms debug_uniforms;         /**< Cached locations. */

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
void scene_render(Scene* scene, mat4 view, mat4 proj, vec3 camera_pos,
                  mat4 previous_view_proj, int width, int height);

/**
 * @brief Updates GPU buffers for dynamic geometry (e.g. LOD changes).
 * @param scene Pointer to the scene.
 */
void scene_update_gpu_buffers(Scene* scene);

#endif /* SCENE_H */
