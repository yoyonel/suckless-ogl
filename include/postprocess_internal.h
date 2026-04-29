#ifndef POSTPROCESS_INTERNAL_H
#define POSTPROCESS_INTERNAL_H

/**
 * @file postprocess_internal.h
 * @brief Internal declarations shared across postprocess_*.c translation units.
 *
 * NOT part of the public API — do not include from outside src/postprocess_*.c.
 */

#include "gpu_profiler.h"
#include "postprocess.h"
#include "pp_ubo.h"
#include "shader.h"
#include <stdbool.h>

/* ---- Texture Units (shared across init, apply, shader TUs) ---- */
enum {
	POSTPROCESS_TEX_UNIT_SCENE = 0,
	POSTPROCESS_TEX_UNIT_BLOOM = 1,
	POSTPROCESS_TEX_UNIT_DEPTH = 2,
	POSTPROCESS_TEX_UNIT_EXPOSURE = 3,
	POSTPROCESS_TEX_UNIT_VELOCITY = 4,
	POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX = 5,
	POSTPROCESS_TEX_UNIT_DOF_BLUR = 6,
	POSTPROCESS_TEX_UNIT_STENCIL = 7,
	POSTPROCESS_TEX_UNIT_LUT3D = 8
};

/* ---- Internal helpers shared across TUs ---- */

/** Destroy the main scene FBO and its attachments. */
void pp_destroy_framebuffer(PostProcess* post_processing);

/** Destroy the fullscreen quad VAO/VBO. */
void pp_destroy_screen_quad(PostProcess* post_processing);

/** Destroy PBO readback buffers and sync objects. */
void pp_destroy_readback_buffers(PostProcess* post_processing);

/** Destroy all cached shader variants. */
void pp_destroy_cached_shaders(PostProcess* post_processing);

/** Check whether a shader is in the variant cache. */
bool pp_is_shader_in_cache(PostProcess* post_processing, Shader* shader);

/** Create the main scene FBO with color, velocity, depth/stencil. */
int pp_create_framebuffer(PostProcess* post_processing);

/** Bind sampler uniforms to texture units on the current shader. */
void pp_setup_sampler_uniforms(PostProcess* post_processing);

/** Update the active shader, destroying the old one if not cached. */
void pp_update_current_shader(PostProcess* post_processing, Shader* new_shader,
                              bool is_optimized);

#endif /* POSTPROCESS_INTERNAL_H */
