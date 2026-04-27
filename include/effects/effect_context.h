/**
 * @file effect_context.h
 * @brief Shared context passed to post-processing effects.
 *
 * EffectContext is the seam between the PostProcess infrastructure
 * (FBOs, textures, timing) and individual effect modules.  Effects
 * receive only what they need — no back-pointer to the full pipeline.
 */

#ifndef EFFECT_CONTEXT_H
#define EFFECT_CONTEXT_H

#include "gl_common.h"

/**
 * @struct EffectContext
 * @brief Read-only snapshot of pipeline state for a single frame.
 */
typedef struct EffectContext {
	GLuint src_tex;      /**< Input color texture (scene HDR). */
	int width;           /**< Current viewport width. */
	int height;          /**< Current viewport height. */
	float delta_time;    /**< Frame delta (seconds). */
	GLuint depth_tex;    /**< Scene depth texture. */
	GLuint velocity_tex; /**< Motion vector texture. */
	float exposure;      /**< Current exposure value. */
} EffectContext;

#endif /* EFFECT_CONTEXT_H */
