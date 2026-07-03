#ifndef SCENE_UNIFORMS_H
#define SCENE_UNIFORMS_H

/**
 * @file scene_uniforms.h
 * @brief Shader uniform cache structs and GPU UBO layouts for scene rendering.
 *
 * These types are implementation details of the rendering pipeline.
 * They are separated from scene.h to keep the public Scene interface lean.
 * Only scene_init.c, scene_render.c, and scene_cleanup.c need these details.
 */

#include "gl_common.h"
#include "light_probes.h" /* SH_TEXTURE_COUNT */

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

#endif /* SCENE_UNIFORMS_H */
