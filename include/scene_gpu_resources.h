#ifndef SCENE_GPU_RESOURCES_H
#define SCENE_GPU_RESOURCES_H

/**
 * @file scene_gpu_resources.h
 * @brief GPU resource handles extracted from Scene to reduce fan-out.
 *
 * Owns all GLuint handles for textures, buffers, VAOs, and compute programs
 * that were previously scattered across the Scene struct.
 */

#include "gl_common.h"
#include "light_probes.h" /* SH_TEXTURE_COUNT */

enum { IBL_TEXTURE_COUNT = 3 };
enum { TEXTURE_UNIT_IBL_START = 15 };

/**
 * @struct SceneGPUResources
 * @brief All GPU resource handles owned by the scene.
 */
typedef struct SceneGPUResources {
	/* --- Geometry Buffers --- */
	GLuint icosphere_vao; /**< Shared icosphere geometry VAO. */
	GLuint icosphere_vbo; /**< Shared icosphere vertex buffer. */
	GLuint icosphere_nbo; /**< Shared icosphere normal buffer. */
	GLuint icosphere_ebo; /**< Shared icosphere index buffer. */
	GLuint quad_vbo;      /**< Shared full-screen quad (FSQ). */
	GLuint wire_cube_vbo; /**< Shared wireframe cube. */
	GLuint wire_quad_vbo; /**< Shared wireframe quad. */
	GLuint empty_vao;     /**< Vertex-less drawing VAO. */

	/* --- Textures --- */
	GLuint hdr_texture;             /**< Active HDR cubemap. */
	GLuint recycled_hdr_tex;        /**< Recycled texture for next load. */
	GLuint spec_prefiltered_tex;    /**< Active Specular map. */
	GLuint irradiance_tex;          /**< Active Irradiance map. */
	GLuint brdf_lut_tex;            /**< Shared BRDF lookup table. */
	GLuint dummy_black_tex;         /**< Safe fallback (0,0,0,1). */
	GLuint dummy_white_tex;         /**< Safe fallback (1,1,1,1). */
	GLuint transition_snapshot_tex; /**< For crossfade mode. */

	/* --- Compute Programs --- */
	GLuint spmap_program;     /**< Internal IBL specular shader. */
	GLuint irmap_program;     /**< Internal IBL irradiance shader. */
	GLuint lum_pass1_program; /**< Luminance downsample pass. */
	GLuint lum_pass2_program; /**< Mean luminance compute pass. */

	/* --- Storage Buffers --- */
	GLuint lum_ssbo[2]; /**< Double-buffered storage for luminance. */

	/* --- Billboard UBO --- */
	GLuint billboard_ubo;    /**< UBO for billboard per-frame uniforms. */
	void* billboard_ubo_ptr; /**< Persistent mapped CPU pointer for UBO. */

	/* --- IBL Binding Cache (Tier 5 — units 15-17) --- */
	GLuint bound_ibl_textures[IBL_TEXTURE_COUNT]; /**< Last IBL active. */

	/* --- SH/Probe Binding Cache (Tier 3 — units 8-14 + SSBO 3) --- */
	GLuint bound_sh_textures[SH_TEXTURE_COUNT]; /**< Last SH tex bound. */
	GLuint bound_probe_ssbo; /**< Last probe SSBO bound. */
} SceneGPUResources;

#endif /* SCENE_GPU_RESOURCES_H */
