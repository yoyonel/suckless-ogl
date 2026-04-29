#ifndef PP_UBO_H
#define PP_UBO_H

/**
 * @file pp_ubo.h
 * @brief GPU-side Uniform Buffer Object layout for the post-processing
 * uber-shader.
 *
 * Must match `layout(std140)` in the GLSL uber-shader.
 */

#include "gl_common.h"
#include <cglm/types.h>
#include <stdint.h>

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
	float dof_anamorphic_ratio;

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
	vec3 fog_color;
	float _pad13;

	/* 3D LUT (16 bytes) */
	float lut3d_intensity;
	float _pad14[3];
} GL_UBO_ALIGNED PostProcessUBO;

GL_ASSERT_UBO_ALIGNMENT(PostProcessUBO);

#endif /* PP_UBO_H */
