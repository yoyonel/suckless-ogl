/**
 * @file postprocess.h
 * @brief Public API for the post-processing pipeline.
 *
 * Provides lifecycle management and render-pass control. For struct internals,
 * include postprocess_internal.h. For setter/getter APIs, include
 * postprocess_setters.h. For readback/time APIs, include
 * postprocess_readback.h.
 */

#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include <glad/glad.h>

typedef struct PostProcess PostProcess;
typedef struct PostProcessPreset PostProcessPreset;
typedef struct GPUProfiler GPUProfiler;

/* --- Lifecycle --- */

int postprocess_init(PostProcess* post_processing,
                     GPUProfiler* external_profiler, int width, int height);
void postprocess_cleanup(PostProcess* post_processing);
void postprocess_compile_optimized(PostProcess* post_processing,
                                   unsigned int static_flags);
void postprocess_use_dynamic(PostProcess* post_processing);
void postprocess_set_dummy_textures(PostProcess* post_processing,
                                    GLuint dummy_black);
void postprocess_resize(PostProcess* post_processing, int width, int height);

/* --- Render Pass Management --- */

void postprocess_begin(PostProcess* post_processing);
void postprocess_end(PostProcess* post_processing);

/* --- Presets --- */

void postprocess_apply_preset(PostProcess* post_processing,
                              const PostProcessPreset* preset);

/* Sub-API headers for specific consumers */
#include "postprocess_readback.h"
#include "postprocess_setters.h"

#endif /* POSTPROCESS_H */
