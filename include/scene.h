#ifndef SCENE_H
#define SCENE_H

#include "app_settings.h"
#include "billboard_rendering.h"
#include "billboard_sorting.h"
#include "gl_common.h"
#include "gpu_profiler.h"
#include "ibl_coordinator.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "light_probes.h"
#include "material.h"
#include "nbody.h"
#include "shader.h"
#include "skybox.h"
#include "trail_renderer.h"
#include <cglm/cglm.h>

#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif

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

enum { IBL_TEXTURE_COUNT = 3 };
enum { TEXTURE_UNIT_IBL_START = 15 };

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
 * @struct BillboardUBO
 * @brief GPU-side uniform buffer for billboard rendering (std140 layout).
 * @note Must match layout(std140, binding = 1) in billboard_ubo.glsl.
 */
enum { MAT4_FLOAT_COUNT = 16 };
typedef struct {
	float projection[MAT4_FLOAT_COUNT]; /**< mat4 projection (offset   0) */
	float view[MAT4_FLOAT_COUNT];       /**< mat4 view       (offset  64) */
	float previous_view_proj[MAT4_FLOAT_COUNT]; /**< mat4 prevVP (offset
	                                               128) */
	float cam_pos[3];            /**< vec3 camPos       (offset 192) */
	int32_t debug_mode;          /**< int  debugMode    (offset 204) */
	float screen_size[2];        /**< vec2 screenSize   (offset 208) */
	float _pad0[2];              /**< alignment pad     (offset 216) */
	float probe_grid_min[3];     /**< vec3 probeGridMin (offset 224) */
	int32_t gi_mode;             /**< int  u_GIMode     (offset 236) */
	float probe_grid_max[3];     /**< vec3 probeGridMax (offset 240) */
	int32_t specular_aa_enabled; /**< int specularAA   (offset 252) */
	int32_t probe_grid_dim[3];   /**< ivec3 probeGridDim(offset 256) */
	int32_t aa_mode;             /**< int  u_aaMode     (offset 268) */
	float grid_to_idx_scale[3];  /**< vec3 gridToIdx    (offset 272) */
	float _pad1;                 /**< alignment pad     (offset 284) */
} GL_UBO_ALIGNED BillboardUBO;

GL_ASSERT_UBO_ALIGNMENT(BillboardUBO);

/**
 * @struct BillboardUniforms
 * @brief Cached uniform locations for billboard rendering.
 * @note Per-frame uniforms are now in BillboardUBO (binding = 1).
 *       Only SH sampler locations remain here (set once at init).
 */
typedef struct {
	GLint
	    sh_textures[SH_TEXTURE_COUNT]; /**< Locations of 'u_SHTexture0-6' */
} BillboardUniforms;

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
	    pbr_billboard_shader; /**< Shader for volumetric/alpha spheres. */
#ifdef USE_SSBO_RENDERING
	Shader* pbr_ssbo_shader; /**< Optimized SSBO shader. */
#endif
	Shader* debug_shader;      /**< Generic debug/visualization shader. */
	Shader* debug_line_shader; /**< Shader for wireframe lines. */
	Shader* skybox_shader;     /**< Skybox shader wrapper. */

	/* --- GPU Resources --- */
	GLuint icosphere_vao;        /**< Shared icosphere geometry VAO. */
	GLuint icosphere_vbo;        /**< Shared icosphere vertex buffer. */
	GLuint icosphere_nbo;        /**< Shared icosphere normal buffer. */
	GLuint icosphere_ebo;        /**< Shared icosphere index buffer. */
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
	GLuint billboard_ubo;    /**< UBO for billboard per-frame uniforms. */
	void* billboard_ubo_ptr; /**< Persistent mapped CPU pointer for UBO. */

	/* --- IBL Binding Cache (Tier 5 — units 15-17) --- */
	GLuint bound_ibl_textures[IBL_TEXTURE_COUNT]; /**< Last IBL active. */

	/* --- SH/Probe Binding Cache (Tier 3 — units 8-14 + SSBO 3) --- */
	GLuint bound_sh_textures[SH_TEXTURE_COUNT]; /**< Last SH tex bound. */
	GLuint bound_probe_ssbo; /**< Last probe SSBO bound. */

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

	/* --- N-Body Simulation --- */
	NBodySim nbody_sim;           /**< N-body gravitational simulation. */
	TrailRenderer trail_renderer; /**< Orbital trail renderer. */
	int nbody_mode;               /**< Toggle: 0=grid, 1=N-body. */

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
