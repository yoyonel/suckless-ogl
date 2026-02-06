/**
 * @file app_settings.h
 * @brief Global application constants, configuration macros, and default
 * values.
 *
 * This header serves as the "Control Panel" for the application. It
 * centralizes:
 * - Feature flags (Compile-time logic switches).
 * - Render quality settings (MSAA, Resolution).
 * - Gameplay/Camera constraints.
 * - Physics and Material default values.
 * - UI styling conventions.
 *
 * @note Including this file does not trigger any heavy dependency. It contains
 * only simple scalar constants and macros.
 */

#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <cglm/types.h>

/**
 * @defgroup Renderer Renderer Configuration
 * @brief Core rendering settings (MSAA, Buffers, Feature Flags).
 * @{
 */

/**
 * @brief Multisample Anti-Aliasing (MSAA) sample count.
 *
 * Used in `app_init()` to configure the GLFW window buffer.
 * - 1: No MSAA (Aliased, fastest).
 * - 4: Standard Quality.
 * - 8 or 16: High Quality (Costly).
 *
 * @note Only affects the main framebuffer (forward rendering geometry).
 * Does not affect offscreen buffers (Post-Process, IBL) which are
 * single-sampled.
 */
enum { DEFAULT_SAMPLES = 1 };

/**
 * @brief Value for all bits enabled in a stencil mask.
 */
static const unsigned int DEFAULT_STENCIL_MASK = 0xFF;

/**
 * @brief Enable High Quality Transparent Sphere Rendering.
 *
 * **If Defined**:
 * - Spheres are rendered as **Transparent** billboards (`GL_BLEND` enabled).
 * - Requires CPU-side sorting (Back-to-Front) every frame in `app_render()`.
 * - Uses `sphere_sorting.c` module.
 * - Alpha channel allows true see-through glass effects.
 *
 * **If Undefined (Legacy/Fast Calculation)**:
 * - Spheres are rendered as **Opaque** (`GL_DEPTH_TEST` enabled, Write
 * enabled).
 * - No sorting required (Fast).
 * - Alpha channel is hijacked to store Luminance for FXAA optimization.
 *
 * @see sphere_rendering.md
 */
#define USE_TRANSPARENT_BILLBOARDS

/** @} */

/**
 * @defgroup Geometry Geometry Generation
 * @brief Icosphere generation limits.
 * @{
 */
enum {
	/** Minimum recursive subdivision level for the icosphere (12 vertices).
	 */
	MIN_SUBDIV = 0,
	/** Maximum subdivision level. Level 6 generates ~40k vertices per
	   sphere. */
	MAX_SUBDIV = 6,
	/**
	 * @deprecated Unused since the switch to Equirectangular mapping.
	 * Kept for ABI compatibility or potential future shadow maps.
	 */
	CUBEMAP_SIZE = 1024,
	/** Starting subdivision level when the app launches. */
	INITIAL_SUBDIVISIONS = 3
};
/** @} */

/**
 * @defgroup Camera Camera Configuration
 * @brief Default starting position and movement constraints.
 * @{
 */

/* Starting State */
static const float DEFAULT_CAMERA_DISTANCE =
    20.0F; /**< Initial orbit radius. */
static const float DEFAULT_CAMERA_YAW =
    -90.0F; /**< Initial horizontal angle (looking -Z). */
static const float DEFAULT_CAMERA_PITCH =
    0.0F; /**< Initial vertical angle (Horizon). */
static const float DEFAULT_ENV_LOD =
    0.0F; /**< Initial Skybox Blur level (0=Sharp). */

/* Projection Matrix */
static const float NEAR_PLANE =
    0.1F; /**< Z-Near: Objects closer are clipped. */
static const float FAR_PLANE = 1000.0F; /**< Z-Far: Objects further are clipped
                                           (Skybox is at 1.0 via trick). */
static const float FOV_ANGLE =
    60.0F; /**< Field of View in degrees (Vertical). */

/* Gameplay Constraints */
static const float MIN_CAMERA_DISTANCE =
    1.5F; /**< Collision sphere radius (Closest zoom). */
static const float MAX_CAMERA_DISTANCE = 50.0F; /**< Furthest zoom allowed. */
static const float ZOOM_STEP = 0.2F; /**< Zoom speed per scroll tick. */
/** @} */

/**
 * @defgroup Environment Environment & Lighting
 * @brief PBR Environment Map settings.
 * @{
 */

/* LOD Controls */
static const float MAX_ENV_LOD =
    10.0F; /**< Max hardware mip level for the HDR texture. */
static const float MIN_ENV_LOD = 0.0F; /**< Mip level 0 (Full res). */
static const float LOD_STEP =
    0.5F; /**< Increment step when pressing PgUp/PgDn. */

/* Directional Light (Sun) - Currently static */
static const float LIGHT_DIR_X = 0.5F;
static const float LIGHT_DIR_Y = 1.0F;
static const float LIGHT_DIR_Z = 0.3F;

/* IBL Map Resolutions (See progressive_ibl.md) */
static const int PREFILTERED_SPECULAR_MAP_SIZE =
    1024; /**< Size of the reflection map (Split-Frame generated). */
static const int IRIDIANCE_MAP_SIZE =
    64; /**< Size of the diffuse irradiance map. */
static const int BRDF_LUT_MAP_SIZE =
    512; /**< Size of the BRDF Lookup Texture (Generated once). */
/** @} */

/**
 * @defgroup PBR PBR Defaults
 * @brief Fallback material parameters when no texture is present.
 * @{
 */
static const float DEFAULT_CLAMP_MULTIPLIER =
    3.0F; /**< HDR clamping threshold for bloom. */
static const float DEFAULT_METALLIC = 1.0F;  /**< Default: Metal (1.0). */
static const float DEFAULT_ROUGHNESS = 0.0F; /**< Default: Smooth (0.0). */
static const float DEFAULT_AO =
    1.0F; /**< Default: Full Ambient Occlusion (1.0). */
/** @} */

/**
 * @defgroup UI User Interface
 * @brief Font, HUD, and Overlay settings.
 * @{
 */
static const float DEFAULT_FONT_SIZE = 32.0F; /**< Base font size in pixels. */
static const float DEFAULT_FPS_SMOOTHING =
    0.95F; /**< EMA Factor for FPS counter (Higher = Smoother). */
static const float DEFAULT_FPS_WINDOW =
    5.0F; /**< Refresh rate of the FPS text (Hz). */
static const int DEFAULT_FPS_SAMPLER_SIZE = 200; /**< History size. */
static const float DEFAULT_FPS_TARGET = 60.0F;   /**< Target FPS. */

/* Instancing Grid Layout (Scene generation) */
static const int DEFAULT_COLS = 10;        /**< Grid width (N x N spheres). */
static const float DEFAULT_SPACING = 2.5F; /**< World units between spheres. */
static const float HALF_OFFSET_MULTIPLIER = 0.5F;

/* Text Aesthetics (Drop Shadow) */
static const float DEFAULT_FONT_SHADOW_OFFSET_X = 2.0F;
static const float DEFAULT_FONT_SHADOW_OFFSET_Y = 2.0F;
static const float DEFAULT_FONT_OFFSET_X = 0.0F;
static const float DEFAULT_FONT_OFFSET_Y = 0.0F;
static const vec3 DEFAULT_FONT_COLOR = {1.0F, 1.0F, 1.0F}; /**< White text. */
static const vec3 DEFAULT_FONT_SHADOW_COLOR = {0.0F, 0.0F,
                                               0.0F}; /**< Black shadow. */
static const int MAX_FPS_TEXT_LENGTH = 64;
/** @} */

/**
 * @defgroup PostProcess Post-Processing Limits
 * @brief Constraints for Tone Mapping and Auto-Exposure.
 * @{
 */
static const float DEFAULT_EXPOSURE_STEP =
    0.1F; /**< Manual exposure adjustment step. */
static const float DEFAULT_MIN_EXPOSURE =
    0.1F; /**< Minimum manual exposure value. */
static const float DEFAULT_AUTO_THRESHOLD =
    5.0F; /**< Threshold for Bloom trigger. */
/** @} */

/**
 * @defgroup Histogram Luminance Histogram
 * @brief Configuration for the auto-exposure histogram.
 * @{
 */
static const int LUM_HISTOGRAM_MAP_SIZE =
    64; /**< Size of the downsampled texture. */
static const int LUM_HISTOGRAM_SIZE =
    64 * 64; /**< Total pixels in the histogram map. */
/** @} */

//
static const int GPU_PROFILER_TOTAL_FRAME_COLOR =
    0xECEFF4;                                         /* Nord Snow Storm */
static const int GPU_PROFILER_ENV_COLOR = 0x88C0D0;   /* Nord Frost Blue */
static const int GPU_PROFILER_SCENE_COLOR = 0xD08770; /* Nord Aurora Orange */
static const int GPU_PROFILER_AUTO_EXPOSURE_COLOR =
    0xEBCB8B;                                         /* Nord Aurora Yellow */
static const int GPU_PROFILER_BLOOM_COLOR = 0x5E81AC; /* Nord Frost Dark Blue */
static const int GPU_PROFILER_DOF_COLOR = 0xA3BE8C;   /* Nord Aurora Green */
static const int GPU_PROFILER_MOTION_BLUR_COLOR =
    0xBF616A; /* Nord Aurora Red */
static const int GPU_PROFILER_POSTPROCESS_COLOR =
    0xB48EAD; /* Nord Aurora Purple */

#endif /* APP_SETTINGS_H */
