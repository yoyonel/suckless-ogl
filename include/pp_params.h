#ifndef PP_PARAMS_H
#define PP_PARAMS_H

/**
 * @file pp_params.h
 * @brief Parameter structs and defaults for uber-shader post-processing
 * effects.
 *
 * Contains configuration structs for effects that run inside the combined
 * uber-shader pass (Vignette, Grain, Exposure, ChromAbbr, WhiteBalance,
 * ColorGrading, Tonemap, FXAA, Banding, Fog). Effects with their own
 * multi-pass pipeline (Bloom, DoF, AutoExposure, MotionBlur, LUT3D)
 * define their params in their respective fx_*.h headers.
 */

#include <cglm/types.h>
#include <stdint.h>

/* --- DEFAULT VALUES --- */
#define DEFAULT_VIGNETTE_INTENSITY 0.8F  /**< Default vignette strength. */
#define DEFAULT_VIGNETTE_SMOOTHNESS 0.5F /**< Default vignette falloff. */
#define DEFAULT_VIGNETTE_ROUNDNESS 1.0F  /**< Default vignette shape. */
#define DEFAULT_GRAIN_INTENSITY 0.02F    /**< Default film grain strength. */
#define DEFAULT_GRAIN_SHADOWS_MAX 0.09F
#define DEFAULT_GRAIN_HIGHLIGHTS_MIN 0.5F
#define DEFAULT_GRAIN_TEXEL_SIZE 1.0F
#define DEFAULT_EXPOSURE 1.00F
#define DEFAULT_CHROM_ABBR_STRENGTH 0.005F

/* FXAA Defaults */
#define DEFAULT_FXAA_SUBPIX 0.75F
#define DEFAULT_FXAA_EDGE_THRESHOLD 0.125F
#define DEFAULT_FXAA_EDGE_THRESHOLD_MIN 0.063F

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

#endif /* PP_PARAMS_H */
