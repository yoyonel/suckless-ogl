#ifndef PP_GPU_RESOURCES_H
#define PP_GPU_RESOURCES_H

/**
 * @file pp_gpu_resources.h
 * @brief GPU resource handles for the post-processing pipeline.
 */

#include "gl_common.h"

/**
 * @struct PPGPUResources
 * @brief GPU handles for the post-processing pipeline (FBOs, textures, UBO).
 */
typedef struct {
	GLuint scene_fbo;          /**< Main HDR framebuffer. */
	GLuint scene_color_tex;    /**< RGBA16F HDR texture. */
	GLuint velocity_tex;       /**< RG16F Motion vector texture. */
	GLuint scene_depth_tex;    /**< D32F Depth texture. */
	GLuint scene_stencil_view; /**< Stencil view of depth texture. */
	GLuint screen_quad_vao;    /**< Shared quad for passes. */
	GLuint screen_quad_vbo;    /**< Quad vertices. */
	GLuint settings_ubo;       /**< GPU buffer for parameters. */
	GLuint dummy_black_tex;    /**< Fallback texture. */
	GLuint dummy_uint_tex;     /**< Fallback unsigned-int texture. */
} PPGPUResources;

#endif /* PP_GPU_RESOURCES_H */
