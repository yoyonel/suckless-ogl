/**
 * @file postprocess.h
 * @brief High-level post-processing pipeline and effects.
 *
 * This module manages the multi-pass post-processing pipeline, including
 * blooming, auto-exposure, color grading, motion blur, and tone mapping.
 * It uses a centralized Uniform Buffer Object (UBO) for settings.
 */

#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_motion_blur.h"
#include "gl_common.h"
#include "gpu_profiler.h"
#include "shader.h"
#include <cglm/cglm.h>
#include <cglm/types.h>

/* --- DEFAULT VALUES --- */
#define POSTPROCESS_HISTOGRAM_BUCKETS 256

#define DEFAULT_VIGNETTE_INTENSITY 0.8F  /**< Default vignette strength. */
#define DEFAULT_VIGNETTE_SMOOTHNESS 0.5F /**< Default vignette falloff. */
#define DEFAULT_VIGNETTE_ROUNDNESS 1.0F  /**< Default vignette shape. */
#define DEFAULT_GRAIN_INTENSITY 0.02F    /**< Default film grain strength. */
#define DEFAULT_GRAIN_SHADOWS_MAX 0.09F
#define DEFAULT_GRAIN_HIGHLIGHTS_MIN 0.5F
#define DEFAULT_GRAIN_TEXEL_SIZE 1.0F
#define DEFAULT_EXPOSURE 1.00F
#define DEFAULT_CHROM_ABBR_STRENGTH 0.005F
#define DEFAULT_BLOOM_INTENSITY 0.0F
#define DEFAULT_BLOOM_THRESHOLD 1.0F
#define DEFAULT_BLOOM_SOFT_THRESHOLD 0.5F
#define DEFAULT_BLOOM_RADIUS 1.0F

/* FXAA Defaults */
#define DEFAULT_FXAA_SUBPIX 0.75F
#define DEFAULT_FXAA_EDGE_THRESHOLD 0.125F
#define DEFAULT_FXAA_EDGE_THRESHOLD_MIN 0.063F

/* DoF defaults */
#define DEFAULT_DOF_FOCAL_DISTANCE 20.0F
#define DEFAULT_DOF_FOCAL_RANGE 5.0F
#define DEFAULT_DOF_BOKEH_SCALE 10.0F

/* Banding defaults */
#define DEFAULT_BANDING_LEVELS 256.0F /**< 8-bit simulation. */

/* Fog Defaults */
#define DEFAULT_FOG_DENSITY 0.0F
#define DEFAULT_FOG_START 10.0F
#define DEFAULT_FOG_HEIGHT_FALLOFF 0.1F
#define DEFAULT_FOG_COLOR_R 0.5F
#define DEFAULT_FOG_COLOR_G 0.6F
#define DEFAULT_FOG_COLOR_B 0.7F

/* White Balance Defaults */
#define DEFAULT_WB_TEMP 6500.0F
#define DEFAULT_WB_TINT 0.0F

/* Filmic Defaults (Safe Neutrals) */
#define DEFAULT_FILMIC_SLOPE 1.0F
#define DEFAULT_FILMIC_TOE 0.0F
#define DEFAULT_FILMIC_SHOULDER 0.0F
#define DEFAULT_FILMIC_BLACK_CLIP 0.0F
#define DEFAULT_FILMIC_WHITE_CLIP 0.0F

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
} PostProcessEffect;

/** @brief Default mask of active effects. */
#define DEFAULT_ACTIVE_EFFECTS                                                \
	((unsigned int)POSTFX_EXPOSURE | (unsigned int)POSTFX_COLOR_GRADING | \
	 (unsigned int)POSTFX_FXAA)

/**
 * @struct ColorGradingParams
 * @brief Unreal-style color grading parameters.
 */
typedef struct {
	float saturation; /**< 0.0 (Grayscale) to 2.0. */
	float contrast;   /**< 0.0 to 2.0. */
	float gamma;      /**< 0.0 to 2.0. */
	float gain;       /**< 0.0 to 2.0. */
	float offset;     /**< -1.0 to 1.0. */
	float lift;       /**< 0.0 to 1.0. */
} ColorGradingParams;

/**
 * @struct VignetteParams
 * @brief Controls for the screen-edge darkening effect.
 */
typedef struct {
	float intensity;  /**< Strength of the outer shadow. */
	float smoothness; /**< Falloff sharpness. */
	float roundness;  /**< Circle vs Rect shape. */
} VignetteParams;

/**
 * @struct GrainParams
 * @brief Fine-grained controls for film noise.
 */
typedef struct {
	float intensity;            /**< Global grain strength. */
	float intensity_shadows;    /**< Shadow-area scaling. */
	float intensity_midtones;   /**< Mid-tone-area scaling. */
	float intensity_highlights; /**< Highlight-area scaling. */
	float shadows_max;          /**< Max luma for shadow grain. */
	float highlights_min;       /**< Min luma for highlight grain. */
	float texel_size;           /**< Particle scale. */
} GrainParams;

/**
 * @struct ExposureParams
 * @brief Manual exposure tuning.
 */
typedef struct {
	float exposure; /**< Stops of exposure compensation. */
} ExposureParams;

/**
 * @struct ChromAbberationParams
 * @brief Focal-length distortion simulation.
 */
typedef struct {
	float strength; /**< Offset distance for color channels. */
} ChromAbberationParams;

/**
 * @struct WhiteBalanceParams
 * @brief Temperature and tint correction.
 */
typedef struct {
	float temperature; /**< Target color temperature in Kelvin. */
	float tint;        /**< Green-Magenta balance. */
} WhiteBalanceParams;

/**
 * @struct TonemapParams
 * @brief ACES-like filmic tonemapping curve parameters.
 */
typedef struct {
	float slope;      /**< Contrast slope. */
	float toe;        /**< Dark compression. */
	float shoulder;   /**< Bright compression. */
	float black_clip; /**< Absolute black cutoff. */
	float white_clip; /**< Absolute white cutoff. */
} TonemapParams;

/**
 * @struct FXAAParams
 * @brief Parameters for Fast Approximate Anti-Aliasing.
 */
typedef struct {
	float subpix;         /**< Sub-pixel quality (0.0 - 1.0). */
	float edge_threshold; /**< Edge detection threshold (0.063 - 0.333). */
	float edge_threshold_min; /**< Minimum edge threshold (0.0312 - 0.0833).
	                           */
} FXAAParams;

/**
 * @enum BandingMode
 * @brief Styles of color quantization.
 */
typedef enum {
	BANDING_MODE_LINEAR = 0,     /**< Standard uniform (Posterization). */
	BANDING_MODE_DITHERED = 1,   /**< Ordered dithering (Bayer). */
	BANDING_MODE_PERCEPTUAL = 2, /**< Gamma-weighted. */
	BANDING_MODE_CHANNEL = 3,    /**< RGB independent. */
	BANDING_MODE_LUMINANCE = 4   /**< Grayscale quantization + Tint. */
} BandingMode;

/**
 * @struct BandingParams
 * @brief Controls for color banding/quantization.
 */
typedef struct {
	int32_t mode;           /**< Banding algorithm to use. */
	float levels;           /**< Global quantization levels. */
	float dither_strength;  /**< Intensity of the dither pattern. */
	float perceptual_gamma; /**< Gamma curve for perceptual mode. */
	vec3 channel_levels;    /**< Independent RGB levels. */
} BandingParams;

/**
 * @struct FogParams
 * @brief Depth-based atmospheric fog parameters.
 */
typedef struct {
	float density;        /**< Exponential fog density. */
	float start;          /**< Near distance where fog begins. */
	float height_falloff; /**< Vertical attenuation factor. */
	float color[3];       /**< Fog color (linear RGB). */
} FogParams;

/**
 * @struct ShaderCacheEntry
 * @brief Cache entry for optimized shaders.
 */
typedef struct {
	unsigned int flags;
	Shader* shader;
} ShaderCacheEntry;

enum { SHADER_CACHE_SIZE = 64 };

/**
 * @struct PostProcessUBO
 * @brief Shared Uniform Buffer structure for shaders.
 * @note Must match `layout(std140)` in GLSL.
 */
typedef struct {
	uint32_t active_effects;
	float time;
	float screen_texel_size[2]; /**< 1.0 / vec2(width, height) */

	/* Vignette */
	float vignette_intensity;
	float vignette_smoothness;
	float vignette_roundness;
	float _pad1;

	/* Grain */
	float grain_intensity;
	float grain_intensity_shadows;
	float grain_intensity_midtones;
	float grain_intensity_highlights;
	float grain_shadows_max;
	float grain_highlights_min;
	float grain_texel_size;
	float _pad2;

	/* Exposure */
	float exposure_manual;
	float _pad3[3];

	/* Chrom Abbr */
	float chrom_abbr_strength;
	float _pad4[3];

	/* White Balance */
	float wb_temperature;
	float wb_tint;
	float _pad5[2];

	/* Color Grading */
	float grading_saturation;
	float grading_contrast;
	float grading_gamma;
	float grading_gain;
	float grading_offset;
	float grading_lift;
	float _pad6[2];

	/* Tonemapper */
	float tonemap_slope;
	float tonemap_toe;
	float tonemap_shoulder;
	float tonemap_black_clip;
	float tonemap_white_clip;
	float _pad7[3];

	/* Bloom */
	float bloom_intensity;
	float bloom_threshold;
	float bloom_soft_threshold;
	float bloom_radius;

	/* DoF */
	float dof_focal_distance;
	float dof_focal_range;
	float dof_bokeh_scale;
	float _pad8;

	/* Motion Blur */
	float mb_intensity;
	float mb_max_velocity;
	int32_t mb_samples;
	float _pad9;

	/* FXAA */
	float fxaa_quality_subpix;
	float fxaa_quality_edge_threshold;
	float fxaa_quality_edge_threshold_min;
	float _pad10;

	/* Banding (32 bytes) */
	int32_t banding_mode;
	float banding_levels;
	float banding_dither_strength;
	float banding_perceptual_gamma;
	float banding_channel_levels[3];
	float _pad11;

	/* Fog (32 bytes) */
	float fog_density;
	float fog_start;
	float fog_height_falloff;
	float _pad12;
	float fog_color[3];
	float _pad13;
} PostProcessUBO;

/**
 * @struct PostProcess
 * @brief Main pipeline state for post-processing.
 */
typedef struct PostProcess {
	/* FBO principal et textures */
	GLuint scene_fbo;          /**< Main HDR framebuffer. */
	GLuint scene_color_tex;    /**< RGBA16F HDR texture. */
	GLuint velocity_tex;       /**< RG16F Motion vector texture. */
	GLuint scene_depth_tex;    /**< D32F Depth texture. */
	GLuint scene_stencil_view; /**< Stencil view of depth texture. */

	/* Module Resources */
	BloomFX bloom_fx;                /**< Bloom subsystem. */
	DoFFX dof_fx;                    /**< Depth-of-field subsystem. */
	AutoExposureFX auto_exposure_fx; /**< Adaptation subsystem. */
	MotionBlurFX motion_blur_fx;     /**< Blur subsystem. */

	/* Render Utilities */
	GLuint screen_quad_vao; /**< Shared quad for passes. */
	GLuint screen_quad_vbo; /**< Quad vertices. */

	GLuint settings_ubo; /**< GPU buffer for parameters. */

	/* Core Shaders */
	Shader* postprocess_shader;  /**< Main Uber-shader. */
	Shader* tile_max_shader;     /**< Motion blur helper. */
	Shader* neighbor_max_shader; /**< Motion blur helper. */

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

	float time;             /**< Accumulated time for noise/animation. */
	float delta_time;       /**< Last frame delta. */
	GLuint dummy_black_tex; /**< Fallback texture. */
	GLuint dummy_uint_tex;

	bool is_optimized; /**< true if Uber-shader uses static preprocessor
	                      flags. */
	bool ubo_dirty;    /**< true when UBO needs re-upload. */

	unsigned int
	    compiled_flags; /**< Flags used for the current optimized shader. */

	ShaderCacheEntry shader_cache[SHADER_CACHE_SIZE];
	int shader_cache_count;

	GPUProfiler* gpu_profiler;
	int banding_preset_idx; /**< Internal index for preset cycling. */

	/* --- Exposure Readback --- */
	GLuint
	    exposure_pbo[2]; /**< Pixel Buffer Object for mean luma readback. */
	GLuint histogram_pbo[2];  /**< Pixel Buffer Object for luminance
	                             histogram  readback. */
	GLsync exposure_sync[2];  /**< Sync objects to avoid CPU stalls on
	                             exposure  readback. */
	GLsync histogram_sync[2]; /**< Sync objects to avoid CPU stalls on
	                             histogram readback. */

	float current_exposure; /**< Cached exposure from GPU readback. */
	float auto_threshold;   /**< Dynamic exposure target. */

	/* --- Histogram Cache (Avoid UI flickering) --- */
	int last_buckets[POSTPROCESS_HISTOGRAM_BUCKETS];
	float last_min_lum;
	float last_max_lum;
	int last_histogram_updated;

	uint64_t frame_count; /**< Internal frame counter for readback sync. */
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
