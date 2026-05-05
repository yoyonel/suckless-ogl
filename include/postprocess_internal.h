#ifndef POSTPROCESS_INTERNAL_H
#define POSTPROCESS_INTERNAL_H

/**
 * @file postprocess_internal.h
 * @brief PostProcess struct definition and internal helpers.
 *
 * Include this header when you need access to PostProcess struct members.
 * For the public lifecycle/render API only, include postprocess.h instead.
 */

#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_lut3d.h"
#include "effects/fx_lut_viz.h"
#include "effects/fx_motion_blur.h"
#include "gpu_profiler.h"
#include "postprocess.h"
#include "pp_exposure_readback.h"
#include "pp_gpu_resources.h"
#include "pp_params.h"
#include "pp_shader_state.h"
#include "pp_ubo.h"
#include "shader.h"
#include <cglm/types.h>
#include <stdbool.h>

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
struct PostProcess {
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
};

/**
 * @struct PostProcessPreset
 * @brief Snapshot of every configurable parameter in the pipeline.
 */
struct PostProcessPreset {
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
};

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
