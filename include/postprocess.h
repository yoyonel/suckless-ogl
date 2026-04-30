/**
 * @file postprocess.h
 * @brief High-level post-processing pipeline and effects.
 *
 * This module manages the multi-pass post-processing pipeline, including
 * blooming, auto-exposure, color grading, motion blur, and tone mapping.
 * It uses a centralized Uniform Buffer Object (UBO) for settings.
 *
 * Implementation details are split across sub-headers:
 * - pp_params.h — Uber-shader effect parameter structs and defaults
 * - pp_ubo.h — GPU-side UBO layout (std140)
 * - pp_gpu_resources.h — FBO, texture, and UBO handles
 * - pp_shader_state.h — Shader cache and compilation state
 * - pp_exposure_readback.h — Async GPU readback for AE/histogram
 */

#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_lut3d.h"
#include "effects/fx_lut_viz.h"
#include "effects/fx_motion_blur.h"
#include "pp_exposure_readback.h"
#include "pp_gpu_resources.h"
#include "pp_params.h"
#include "pp_shader_state.h"
#include <cglm/types.h>

typedef struct GPUProfiler GPUProfiler;

/* --- FX-specific DEFAULT VALUES (owned by PostProcess init) --- */
#define DEFAULT_BLOOM_INTENSITY 0.0F
#define DEFAULT_BLOOM_THRESHOLD 1.0F
#define DEFAULT_BLOOM_SOFT_THRESHOLD 0.5F
#define DEFAULT_BLOOM_RADIUS 1.0F

/* DoF defaults */
#define DEFAULT_DOF_FOCAL_DISTANCE 20.0F
#define DEFAULT_DOF_FOCAL_RANGE 5.0F
#define DEFAULT_DOF_BOKEH_SCALE 10.0F
#define DEFAULT_DOF_ANAMORPHIC_RATIO \
	1.0F /**< 1.0 = Spherical, 2.0 = Anamorphic */

/**
 * @struct PostProcess
 * @brief Main pipeline state for post-processing.
 */
typedef struct PostProcess {
	/* GPU Resources */
	PPGPUResources gpu; /**< FBOs, textures, UBO, quad. */

	/* Module Resources */
	BloomFX bloom_fx;                /**< Bloom subsystem. */
	DoFFX dof_fx;                    /**< Depth-of-field subsystem. */
	AutoExposureFX auto_exposure_fx; /**< Adaptation subsystem. */
	MotionBlurFX motion_blur_fx;     /**< Blur subsystem. */
	LUT3DFX lut3d_fx;                /**< 3D LUT subsystem. */
	LUTVizFX lut_viz_fx;             /**< LUT visualization. */

	/* Shader Management */
	PPShaderState shaders; /**< Uber-shader pipeline state. */

	int width;  /**< Target resolution width. */
	int height; /**< Target resolution height. */

	unsigned int active_effects; /**< Bitfield of enabled effects. */

	/* Logic Parameters */
	VignetteParams vignette;
	GrainParams grain;
	ExposureParams exposure;
	ChromAbberationParams chrom_abbr;
	WhiteBalanceParams white_balance;
	ColorGradingParams color_grading;
	TonemapParams tonemapper;
	BloomParams bloom;
	DoFParams dof;
	AutoExposureParams auto_exposure;
	MotionBlurParams motion_blur;
	FXAAParams fxaa;
	BandingParams banding;
	FogParams fog;
	LUT3DParams lut3d;

	float time;       /**< Accumulated time for noise/animation. */
	float delta_time; /**< Last frame delta. */
	bool ubo_dirty;   /**< true when UBO needs re-upload. */

	GPUProfiler* gpu_profiler;
	int banding_preset_idx; /**< Internal index for preset cycling. */

	/* Exposure Readback */
	PPExposureReadback readback; /**< Async GPU readback state. */
} PostProcess;

/* --- Lifecycle --- */

/**
 * @brief Initializes the post-processing pipeline.
 * @param post_processing Pointer to the struct.
 * @param external_profiler Pointer to the GPU profiler for timing stages.
 * @param width Initial resolution width.
 * @param height Initial resolution height.
 * @return 0 on success, negative on error.
 */
int postprocess_init(PostProcess* post_processing,
                     GPUProfiler* external_profiler, int width, int height);

/**
 * @brief Releases all GPU and CPU resources.
 * @param post_processing Pointer to the struct.
 */
void postprocess_cleanup(PostProcess* post_processing);

/**
 * @brief Compiles a specialized Uber-shader for maximum performance.
 * @param post_processing Pointer to the struct.
 * @param static_flags Bitmask of effects to bake into the shader.
 */
void postprocess_compile_optimized(PostProcess* post_processing,
                                   unsigned int static_flags);

/**
 * @brief Switches back to the dynamic/generic Uber-shader.
 * @param post_processing Pointer to the struct.
 */
void postprocess_use_dynamic(PostProcess* post_processing);

/** @brief Internal helper to set fallback textures. */
void postprocess_set_dummy_textures(PostProcess* post_processing,
                                    GLuint dummy_black);

/**
 * @brief Recreates all internal buffers for a new resolution.
 * @param post_processing Pointer to the struct.
 * @param width New width.
 * @param height New height.
 */
void postprocess_resize(PostProcess* post_processing, int width, int height);

/* --- Render Pass Management --- */

/**
 * @brief Binds the HDR FBO and prepares for scene rendering.
 * Should be called BEFORE the main render loop.
 */
void postprocess_begin(PostProcess* post_processing);

/**
 * @brief Processes the HDR scene and renders the final LDR result to screen.
 * Should be called AFTER the main render loop.
 */
void postprocess_end(PostProcess* post_processing);

/**
 * @struct PostProcessPreset
 * @brief Snapshot of every configurable parameter in the pipeline.
 */
typedef struct {
	unsigned int active_effects;
	VignetteParams vignette;
	GrainParams grain;
	ExposureParams exposure;
	ChromAbberationParams chrom_abbr;
	WhiteBalanceParams white_balance;
	ColorGradingParams color_grading;
	TonemapParams tonemapper;
	BloomParams bloom;
	DoFParams dof;
	FXAAParams fxaa;
	BandingParams banding;
	FogParams fog;
	LUT3DParams lut3d;
} PostProcessPreset;

/**
 * @brief Applies all settings from a preset atomically.
 * @param post_processing Pointer to the struct.
 * @param preset Pointer to the preset values.
 * @see postprocess_presets.h
 */
void postprocess_apply_preset(PostProcess* post_processing,
                              const PostProcessPreset* preset);

/* Sub-API headers for specific consumers */
#include "postprocess_readback.h"
#include "postprocess_setters.h"

#endif /* POSTPROCESS_H */
