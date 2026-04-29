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
#include "gpu_profiler.h"
#include "pp_exposure_readback.h"
#include "pp_gpu_resources.h"
#include "pp_params.h"
#include "pp_shader_state.h"
#include "pp_ubo.h"
#include <cglm/cglm.h>

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
 * @enum PostProcessEffect
 * @brief Bitmask flags for enabling/disabling individual effects.
 */
typedef enum {
	POSTFX_VIGNETTE = (1U << 0U),   /**< Vignette overlay. */
	POSTFX_GRAIN = (1U << 1U),      /**< Film grain noise. */
	POSTFX_EXPOSURE = (1U << 2U),   /**< Manual exposure compensation. */
	POSTFX_CHROM_ABBR = (1U << 3U), /**< Chromatic aberration. */
	POSTFX_BLOOM = (1U << 4U),      /**< HDR Bloom. */
	POSTFX_COLOR_GRADING =
	    (1U << 5U),          /**< Saturation/Contrast/Gamma adjustment. */
	POSTFX_DOF = (1U << 6U), /**< Depth of Field. */
	POSTFX_DOF_DEBUG = (1U << 7U), /**< Focus visualization. */
	POSTFX_AUTO_EXPOSURE =
	    (1U << 8U), /**< Automatic exposure adaptation. */
	POSTFX_EXPOSURE_DEBUG =
	    (1U << 9U), /**< Exposure histogram visualization. */
	POSTFX_MOTION_BLUR = (1U << 10U), /**< Velocity-based motion blur. */
	POSTFX_MOTION_BLUR_DEBUG =
	    (1U << 11U),                 /**< Velocity buffer visualization. */
	POSTFX_FXAA = (1U << 12U),       /**< Fast Approximate Anti-Aliasing. */
	POSTFX_FXAA_DEBUG = (1U << 13U), /**< Edge detection visualization. */
	POSTFX_BANDING = (1U << 14U),    /**< Color banding/quantization. */
	POSTFX_VECTOR_FIELD_DEBUG =
	    (1U << 15U), /**< Vector field velocity visualization. */
	POSTFX_STENCIL_DEBUG = (1U << 16U), /**< Stencil mask visualization. */
	POSTFX_BLOOM_DEBUG = (1U << 17U),   /**< Bloom debug view. */
	POSTFX_FOG = (1U << 18U),           /**< Atmospheric depth fog. */
	POSTFX_FOG_DEBUG = (1U << 19U),     /**< Fog component visualization. */
	POSTFX_LUT3D = (1U << 20U),         /**< 3D LUT Gamut Mapping. */
	POSTFX_LUT_VIZ = (1U << 21U), /**< 3D LUT Lattice visualization. */
} PostProcessEffect;

/** @brief Default mask of active effects. */
#define DEFAULT_ACTIVE_EFFECTS                                                \
	((unsigned int)POSTFX_EXPOSURE | (unsigned int)POSTFX_COLOR_GRADING | \
	 (unsigned int)POSTFX_FXAA)

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

/* --- Effect Control --- */

/** @brief Enables a specific effect. */
void postprocess_enable(PostProcess* post_processing, PostProcessEffect effect);
/** @brief Disables a specific effect. */
void postprocess_disable(PostProcess* post_processing,
                         PostProcessEffect effect);
/** @brief Toggles the current state of an effect. */
void postprocess_toggle(PostProcess* post_processing, PostProcessEffect effect);
/** @brief Returns true if an effect is currently active. */
int postprocess_is_enabled(PostProcess* post_processing,
                           PostProcessEffect effect);

/* --- Parameter Tuning --- */

void postprocess_set_white_balance(PostProcess* post_processing,
                                   float temperature, float tint);
void postprocess_set_color_grading(PostProcess* post_processing,
                                   float saturation, float contrast,
                                   float gamma, float gain, float offset,
                                   float lift);
void postprocess_set_tonemapper(PostProcess* post_processing, float slope,
                                float toe, float shoulder, float black_clip,
                                float white_clip);
void postprocess_set_grading_ue_default(PostProcess* post_processing);
void postprocess_set_vignette(PostProcess* post_processing, float intensity,
                              float smoothness, float roundness);
void postprocess_set_grain(PostProcess* post_processing, float intensity);
void postprocess_set_exposure(PostProcess* post_processing, float exposure);
void postprocess_set_chrom_abbr(PostProcess* post_processing, float strength);
void postprocess_set_bloom(PostProcess* post_processing, float intensity,
                           float threshold, float soft_threshold);
void postprocess_set_dof(PostProcess* post_processing, float focal_distance,
                         float focal_range, float bokeh_scale);
void postprocess_set_dof_anamorphic(PostProcess* post_processing,
                                    float anamorphic_ratio);
float postprocess_get_exposure(PostProcess* post_processing);
void postprocess_set_auto_exposure(PostProcess* post_processing,
                                   float min_luminance, float max_luminance,
                                   float speed_up, float speed_down,
                                   float key_value);
void postprocess_set_fxaa(PostProcess* post_processing, float subpix,
                          float edge_threshold, float edge_threshold_min);
void postprocess_set_banding(PostProcess* post_processing, BandingMode mode,
                             float levels);
void postprocess_set_banding_dither(PostProcess* post_processing,
                                    float strength);
void postprocess_set_banding_perceptual(PostProcess* post_processing,
                                        float gamma);
void postprocess_set_banding_channels(PostProcess* post_processing, float red,
                                      float green, float blue);
void postprocess_set_fog(PostProcess* post_processing, float density,
                         float start, float height_falloff, float fog_r,
                         float fog_g, float fog_b);
void postprocess_set_lut3d(PostProcess* post_processing, float intensity,
                           GLuint texture);
int postprocess_load_lut3d(PostProcess* post_processing, const char* path);
/**
 * @brief Updates view-projection matrices for effects requiring
 * depth-reconstruction.
 * @param post_processing Pointer to the struct.
 * @param view_proj The current frame's View-Proj matrix.
 */
void postprocess_update_matrices(PostProcess* post_processing, mat4 view_proj);

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
 * @brief Increments internal clocks.
 * @param post_processing Pointer to the struct.
 * @param delta_time SECONDS elapsed since last frame.
 */
void postprocess_update_time(PostProcess* post_processing, float delta_time);

GLuint postprocess_get_exposure_pbo(PostProcess* post_processing, int index);
GLuint postprocess_get_histogram_pbo(PostProcess* post_processing, int index);
GLsync postprocess_get_exposure_sync(PostProcess* post_processing, int index);
GLsync postprocess_get_histogram_sync(PostProcess* post_processing, int index);
void postprocess_set_exposure_sync(PostProcess* post_processing, int index,
                                   GLsync sync);
void postprocess_set_histogram_sync(PostProcess* post_processing, int index,
                                    GLsync sync);

/**
 * @brief Updates all async GPU readbacks (Exposure, Histogram).
 * Handles PBO mapping and Sync management internally to avoid CPU stalls.
 */
void postprocess_update_readbacks(PostProcess* post_processing,
                                  uint64_t frame_count);

/**
 * @brief Updates the target exposure threshold for AE.
 */
void postprocess_set_exposure_target(PostProcess* post_processing,
                                     float threshold);

/**
 * @brief Computes the luminance histogram from the GPU readback.
 * @return 1 if buckets were updated, 0 otherwise.
 */
int postprocess_compute_luminance_histogram(PostProcess* post_processing,
                                            uint64_t frame_count, int* buckets,
                                            int size, float* min_lum,
                                            float* max_lum);

#endif /* POSTPROCESS_H */
