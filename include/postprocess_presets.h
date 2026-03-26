/**
 * @file postprocess_presets.h
 * @brief Predefined configurations for the post-processing pipeline.
 */

#ifndef POSTPROCESS_PRESETS_H
#define POSTPROCESS_PRESETS_H

#include "postprocess.h"

#define BANDING_COMMON_BASE                                      \
	.active_effects = (unsigned int)POSTFX_EXPOSURE |        \
	                  (unsigned int)POSTFX_BANDING |         \
	                  (unsigned int)POSTFX_FXAA,             \
	.vignette = {.intensity = DEFAULT_VIGNETTE_INTENSITY,    \
	             .smoothness = DEFAULT_VIGNETTE_SMOOTHNESS,  \
	             .roundness = DEFAULT_VIGNETTE_ROUNDNESS},   \
	.grain = {.intensity = DEFAULT_GRAIN_INTENSITY,          \
	          .intensity_shadows = 1.0F,                     \
	          .intensity_midtones = 1.0F,                    \
	          .intensity_highlights = 1.0F,                  \
	          .shadows_max = 0.09F,                          \
	          .highlights_min = 0.5F,                        \
	          .texel_size = 1.0F},                           \
	.exposure = {.exposure = DEFAULT_EXPOSURE},              \
	.chrom_abbr = {.strength = DEFAULT_CHROM_ABBR_STRENGTH}, \
	.white_balance = {.temperature = DEFAULT_WB_TEMP,        \
	                  .tint = DEFAULT_WB_TINT},              \
	.tonemapper = {.slope = DEFAULT_FILMIC_SLOPE,            \
	               .toe = DEFAULT_FILMIC_TOE,                \
	               .shoulder = DEFAULT_FILMIC_SHOULDER,      \
	               .black_clip = DEFAULT_FILMIC_BLACK_CLIP,  \
	               .white_clip = DEFAULT_FILMIC_WHITE_CLIP}, \
	.bloom = {.intensity = 0.0F,                             \
	          .threshold = 1.0F,                             \
	          .soft_threshold = 0.5F,                        \
	          .radius = 1.0F},                               \
	.dof = {.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE,    \
	        .focal_range = DEFAULT_DOF_FOCAL_RANGE,          \
	        .bokeh_scale = DEFAULT_DOF_BOKEH_SCALE},         \
	.fxaa = {.subpix = DEFAULT_FXAA_SUBPIX,                  \
	         .edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD,  \
	         .edge_threshold_min = DEFAULT_FXAA_EDGE_THRESHOLD_MIN}

/** @brief Default balanced settings. */
static const PostProcessPreset PRESET_DEFAULT = {
    .active_effects = (unsigned int)POSTFX_EXPOSURE,
    .vignette = {.intensity = DEFAULT_VIGNETTE_INTENSITY,
                 .smoothness = DEFAULT_VIGNETTE_SMOOTHNESS,
                 .roundness = DEFAULT_VIGNETTE_ROUNDNESS},
    .grain = {.intensity = DEFAULT_GRAIN_INTENSITY,
              .intensity_shadows = 1.0F,
              .intensity_midtones = 1.0F,
              .intensity_highlights = 1.0F,
              .shadows_max = 0.09F,
              .highlights_min = 0.5F,
              .texel_size = 1.0F},
    .exposure = {.exposure = DEFAULT_EXPOSURE},
    .chrom_abbr = {.strength = DEFAULT_CHROM_ABBR_STRENGTH},
    .white_balance = {.temperature = DEFAULT_WB_TEMP, .tint = DEFAULT_WB_TINT},
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .tonemapper = {.slope = DEFAULT_FILMIC_SLOPE,
                   .toe = DEFAULT_FILMIC_TOE,
                   .shoulder = DEFAULT_FILMIC_SHOULDER,
                   .black_clip = DEFAULT_FILMIC_BLACK_CLIP,
                   .white_clip = DEFAULT_FILMIC_WHITE_CLIP},
    .bloom = {.intensity = 0.0F,
              .threshold = 1.0F,
              .soft_threshold = 0.5F,
              .radius = 1.0F},
    .dof = {.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE,
            .focal_range = DEFAULT_DOF_FOCAL_RANGE,
            .bokeh_scale = DEFAULT_DOF_BOKEH_SCALE},
    .fxaa = {.subpix = DEFAULT_FXAA_SUBPIX,
             .edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD,
             .edge_threshold_min = DEFAULT_FXAA_EDGE_THRESHOLD_MIN},
    .banding = {
        .mode = BANDING_MODE_LINEAR,
        .levels = DEFAULT_BANDING_LEVELS,
        .dither_strength = 0.0F,
        .perceptual_gamma = 1.0F,
        .channel_levels = {DEFAULT_BANDING_LEVELS, DEFAULT_BANDING_LEVELS,
                           DEFAULT_BANDING_LEVELS}}};

/** @brief Subtle adjustments. */
static const PostProcessPreset PRESET_SUBTLE = {
    .active_effects =
        (unsigned int)POSTFX_VIGNETTE | (unsigned int)POSTFX_GRAIN |
        (unsigned int)POSTFX_EXPOSURE | (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = 0.3F, .smoothness = 0.5F, .roundness = 1.0F},
    .grain = {.intensity = 0.02F,
              .intensity_shadows = 1.0F,
              .intensity_midtones = 1.0F,
              .intensity_highlights = 1.0F,
              .shadows_max = 0.09F,
              .highlights_min = 0.5F,
              .texel_size = 1.0F},
    .exposure = {.exposure = DEFAULT_EXPOSURE},
    .chrom_abbr = {.strength = 0.01F},
    .white_balance = {.temperature = DEFAULT_WB_TEMP, .tint = DEFAULT_WB_TINT},
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .tonemapper = {.slope = DEFAULT_FILMIC_SLOPE,
                   .toe = DEFAULT_FILMIC_TOE,
                   .shoulder = DEFAULT_FILMIC_SHOULDER,
                   .black_clip = DEFAULT_FILMIC_BLACK_CLIP,
                   .white_clip = DEFAULT_FILMIC_WHITE_CLIP},
    .bloom = {.intensity = 0.02F,
              .threshold = 1.0F,
              .soft_threshold = 0.5F,
              .radius = 1.0F},
    .dof = {.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE,
            .focal_range = DEFAULT_DOF_FOCAL_RANGE,
            .bokeh_scale = DEFAULT_DOF_BOKEH_SCALE},
    .fxaa = {.subpix = DEFAULT_FXAA_SUBPIX,
             .edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD,
             .edge_threshold_min = DEFAULT_FXAA_EDGE_THRESHOLD_MIN},
    .banding = {
        .mode = BANDING_MODE_LINEAR,
        .levels = DEFAULT_BANDING_LEVELS,
        .dither_strength = 0.0F,
        .perceptual_gamma = 1.0F,
        .channel_levels = {DEFAULT_BANDING_LEVELS, DEFAULT_BANDING_LEVELS,
                           DEFAULT_BANDING_LEVELS}}};

/** @brief Rich, high-contrast look. */
static const PostProcessPreset PRESET_CINEMATIC = {
    .active_effects =
        (unsigned int)POSTFX_VIGNETTE | (unsigned int)POSTFX_GRAIN |
        (unsigned int)POSTFX_AUTO_EXPOSURE | (unsigned int)POSTFX_BLOOM |
        (unsigned int)POSTFX_DOF | (unsigned int)POSTFX_COLOR_GRADING |
        (unsigned int)POSTFX_MOTION_BLUR,
    .vignette = {.intensity = 0.5F, .smoothness = 0.6F, .roundness = 0.5F},
    .grain = {.intensity = 0.03F,
              .intensity_shadows = 1.2F,
              .intensity_midtones = 1.0F,
              .intensity_highlights = 0.8F,
              .shadows_max = 0.09F,
              .highlights_min = 0.5F,
              .texel_size = 1.0F},
    .exposure = {.exposure = 1.2F},
    .chrom_abbr = {.strength = 0.015F},
    .white_balance = {.temperature = 6500.0F, .tint = -0.05F},
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .tonemapper = {.slope = DEFAULT_FILMIC_SLOPE,
                   .toe = DEFAULT_FILMIC_TOE,
                   .shoulder = DEFAULT_FILMIC_SHOULDER,
                   .black_clip = DEFAULT_FILMIC_BLACK_CLIP,
                   .white_clip = DEFAULT_FILMIC_WHITE_CLIP},
    .bloom = {.intensity = 0.04F,
              .threshold = 1.0F,
              .soft_threshold = 0.5F,
              .radius = 1.0F},
    .dof = {.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE,
            .focal_range = DEFAULT_DOF_FOCAL_RANGE,
            .bokeh_scale = DEFAULT_DOF_BOKEH_SCALE},
    .fxaa = {.subpix = DEFAULT_FXAA_SUBPIX,
             .edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD,
             .edge_threshold_min = DEFAULT_FXAA_EDGE_THRESHOLD_MIN},
    .banding = {
        .mode = BANDING_MODE_LINEAR,
        .levels = DEFAULT_BANDING_LEVELS,
        .dither_strength = 0.0F,
        .perceptual_gamma = 1.0F,
        .channel_levels = {DEFAULT_BANDING_LEVELS, DEFAULT_BANDING_LEVELS,
                           DEFAULT_BANDING_LEVELS}}};

/** @brief Warm, grainy look. */
static const PostProcessPreset PRESET_VINTAGE = {
    .active_effects =
        (unsigned int)POSTFX_VIGNETTE | (unsigned int)POSTFX_GRAIN |
        (unsigned int)POSTFX_CHROM_ABBR | (unsigned int)POSTFX_EXPOSURE |
        (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = 0.7F, .smoothness = 0.8F, .roundness = 1.0F},
    .grain = {.intensity = 0.06F,
              .intensity_shadows = 1.5F,
              .intensity_midtones = 1.0F,
              .intensity_highlights = 0.5F,
              .shadows_max = 0.15F,
              .highlights_min = 0.6F,
              .texel_size = 1.5F},
    .exposure = {.exposure = 0.9F},
    .chrom_abbr = {.strength = 0.02F},
    .white_balance = {.temperature = 5500.0F, .tint = 0.05F},
    .color_grading = {.saturation = 0.8F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .tonemapper = {.slope = DEFAULT_FILMIC_SLOPE,
                   .toe = DEFAULT_FILMIC_TOE,
                   .shoulder = DEFAULT_FILMIC_SHOULDER,
                   .black_clip = DEFAULT_FILMIC_BLACK_CLIP,
                   .white_clip = DEFAULT_FILMIC_WHITE_CLIP},
    .bloom = {.intensity = 0.0F,
              .threshold = 1.0F,
              .soft_threshold = 0.5F,
              .radius = 1.0F},
    .dof = {.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE,
            .focal_range = DEFAULT_DOF_FOCAL_RANGE,
            .bokeh_scale = DEFAULT_DOF_BOKEH_SCALE},
    .fxaa = {.subpix = DEFAULT_FXAA_SUBPIX,
             .edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD,
             .edge_threshold_min = DEFAULT_FXAA_EDGE_THRESHOLD_MIN},
    .banding = {
        .mode = BANDING_MODE_LINEAR,
        .levels = DEFAULT_BANDING_LEVELS,
        .dither_strength = 0.0F,
        .perceptual_gamma = 1.0F,
        .channel_levels = {DEFAULT_BANDING_LEVELS, DEFAULT_BANDING_LEVELS,
                           DEFAULT_BANDING_LEVELS}}};

/** @brief Cool, green-tinted look. */
static const PostProcessPreset PRESET_MATRIX = {
    .active_effects =
        (unsigned int)POSTFX_COLOR_GRADING | (unsigned int)POSTFX_BLOOM,
    .vignette = {.intensity = DEFAULT_VIGNETTE_INTENSITY,
                 .smoothness = DEFAULT_VIGNETTE_SMOOTHNESS,
                 .roundness = DEFAULT_VIGNETTE_ROUNDNESS},
    .grain = {.intensity = DEFAULT_GRAIN_INTENSITY,
              .intensity_shadows = 1.0F,
              .intensity_midtones = 1.0F,
              .intensity_highlights = 1.0F,
              .shadows_max = 0.09F,
              .highlights_min = 0.5F,
              .texel_size = 1.0F},
    .exposure = {.exposure = DEFAULT_EXPOSURE},
    .chrom_abbr = {.strength = DEFAULT_CHROM_ABBR_STRENGTH},
    .white_balance = {.temperature = 7500.0F, .tint = 0.2F},
    .color_grading = {.saturation = 0.5F,
                      .contrast = 1.2F,
                      .gamma = 0.9F,
                      .gain = 1.1F,
                      .offset = 0.02F},
    .tonemapper = {.slope = DEFAULT_FILMIC_SLOPE,
                   .toe = DEFAULT_FILMIC_TOE,
                   .shoulder = DEFAULT_FILMIC_SHOULDER,
                   .black_clip = DEFAULT_FILMIC_BLACK_CLIP,
                   .white_clip = DEFAULT_FILMIC_WHITE_CLIP},
    .bloom = {.intensity = 0.2F,
              .threshold = 0.8F,
              .soft_threshold = 0.5F,
              .radius = 1.0F},
    .dof = {.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE,
            .focal_range = DEFAULT_DOF_FOCAL_RANGE,
            .bokeh_scale = DEFAULT_DOF_BOKEH_SCALE},
    .fxaa = {.subpix = DEFAULT_FXAA_SUBPIX,
             .edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD,
             .edge_threshold_min = DEFAULT_FXAA_EDGE_THRESHOLD_MIN},
    .banding = {
        .mode = BANDING_MODE_LINEAR,
        .levels = DEFAULT_BANDING_LEVELS,
        .dither_strength = 0.0F,
        .perceptual_gamma = 1.0F,
        .channel_levels = {DEFAULT_BANDING_LEVELS, DEFAULT_BANDING_LEVELS,
                           DEFAULT_BANDING_LEVELS}}};

/** @brief High-contrast B&W. */
static const PostProcessPreset PRESET_BW_CONTRAST = {
    .active_effects = (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = DEFAULT_VIGNETTE_INTENSITY,
                 .smoothness = DEFAULT_VIGNETTE_SMOOTHNESS,
                 .roundness = DEFAULT_VIGNETTE_ROUNDNESS},
    .grain = {.intensity = DEFAULT_GRAIN_INTENSITY,
              .intensity_shadows = 1.0F,
              .intensity_midtones = 1.0F,
              .intensity_highlights = 1.0F,
              .shadows_max = 0.09F,
              .highlights_min = 0.5F,
              .texel_size = 1.0F},
    .exposure = {.exposure = DEFAULT_EXPOSURE},
    .chrom_abbr = {.strength = DEFAULT_CHROM_ABBR_STRENGTH},
    .white_balance = {.temperature = DEFAULT_WB_TEMP, .tint = DEFAULT_WB_TINT},
    .color_grading = {.saturation = 0.0F,
                      .contrast = 1.5F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .tonemapper = {.slope = DEFAULT_FILMIC_SLOPE,
                   .toe = DEFAULT_FILMIC_TOE,
                   .shoulder = DEFAULT_FILMIC_SHOULDER,
                   .black_clip = DEFAULT_FILMIC_BLACK_CLIP,
                   .white_clip = DEFAULT_FILMIC_WHITE_CLIP},
    .bloom = {.intensity = 0.0F,
              .threshold = 1.0F,
              .soft_threshold = 0.5F,
              .radius = 1.0F},
    .dof = {.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE,
            .focal_range = DEFAULT_DOF_FOCAL_RANGE,
            .bokeh_scale = DEFAULT_DOF_BOKEH_SCALE},
    .fxaa = {.subpix = DEFAULT_FXAA_SUBPIX,
             .edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD,
             .edge_threshold_min = DEFAULT_FXAA_EDGE_THRESHOLD_MIN},
    .banding = {
        .mode = BANDING_MODE_LINEAR,
        .levels = DEFAULT_BANDING_LEVELS,
        .dither_strength = 0.0F,
        .perceptual_gamma = 1.0F,
        .channel_levels = {DEFAULT_BANDING_LEVELS, DEFAULT_BANDING_LEVELS,
                           DEFAULT_BANDING_LEVELS}}};

/** @brief Art style: Posterized. */
static const PostProcessPreset PRESET_POSTERIZED = {
    BANDING_COMMON_BASE,
    .color_grading = {.saturation = 1.5F,
                      .contrast = 1.5F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .banding = {.mode = BANDING_MODE_LINEAR,
                .levels = 4.0F,
                .dither_strength = 0.0F,
                .perceptual_gamma = 1.0F,
                .channel_levels = {4.0F, 4.0F, 4.0F}}};

/** @brief Art style: Retro Computing (Dithered). */
static const PostProcessPreset PRESET_RETRO = {
    BANDING_COMMON_BASE,
    .color_grading = {.saturation = 0.5F,
                      .contrast = 1.3F,
                      .gamma = 1.1F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .banding = {.mode = BANDING_MODE_DITHERED,
                .levels = 8.0F,
                .dither_strength = 1.5F,
                .perceptual_gamma = 1.0F,
                .channel_levels = {8.0F, 8.0F, 8.0F}}};

/** @brief Art style: Perceptual (Analog-like). */
static const PostProcessPreset PRESET_ANALOG = {
    BANDING_COMMON_BASE,
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .banding = {.mode = BANDING_MODE_PERCEPTUAL,
                .levels = 12.0F,
                .dither_strength = 0.0F,
                .perceptual_gamma = 2.2F,
                .channel_levels = {12.0F, 12.0F, 12.0F}}};

/** @brief Art style: Channel (VGA/CGA-like). */
static const PostProcessPreset PRESET_CHANNEL_GFX = {
    BANDING_COMMON_BASE,
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .banding = {.mode = BANDING_MODE_CHANNEL,
                .levels = 256.0F,
                .dither_strength = 0.0F,
                .perceptual_gamma = 1.0F,
                .channel_levels = {8.0F, 8.0F, 4.0F}}};

/** @brief Art style: Blueprint / Hologram. */
static const PostProcessPreset PRESET_BLUEPRINT = {
    BANDING_COMMON_BASE,
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F},
    .banding = {.mode = BANDING_MODE_LUMINANCE,
                .levels = 6.0F,
                .dither_strength = 0.0F,
                .perceptual_gamma = 1.0F,
                .channel_levels = {0.1F, 0.4F, 0.9F}}};

/**
 * @brief Nordic Noir: Foggy neon-lit night with teal-orange split toning.
 *
 * Inspired by Nordic noir photography — cool atmospheric fog, warm neon
 * highlights, visible film grain, lifted shadows with teal cast, and
 * filmic tone mapping with compressed dynamic range.
 */
static const PostProcessPreset PRESET_NORDIC_NOIR = {
    .active_effects =
        (unsigned int)POSTFX_VIGNETTE | (unsigned int)POSTFX_GRAIN |
        (unsigned int)POSTFX_EXPOSURE | (unsigned int)POSTFX_BLOOM |
        (unsigned int)POSTFX_COLOR_GRADING | (unsigned int)POSTFX_CHROM_ABBR |
        (unsigned int)POSTFX_FXAA | (unsigned int)POSTFX_FOG,
    /* Vignette: Subtle oval, soft falloff */
    .vignette = {.intensity = 0.45F, .smoothness = 0.7F, .roundness = 0.6F},
    /* Grain: Film-like noise, heavier in shadows */
    .grain = {.intensity = 0.25F,
              .intensity_shadows = 1.3F,
              .intensity_midtones = 1.0F,
              .intensity_highlights = 0.4F,
              .shadows_max = 0.15F,
              .highlights_min = 0.55F,
              .texel_size = 2.0F},
    /* Exposure: Slightly moody/underexposed */
    .exposure = {.exposure = 0.85F},
    /* Chromatic Aberration: Barely visible, organic feel */
    .chrom_abbr = {.strength = 0.003F}, /* White Balance: Colder teal push */
    .white_balance = {.temperature = 4050.0F, .tint = -0.04F},
    /* Color Grading: Noir look with pronounced milky shadows (Lift) */
    .color_grading = {.saturation = 0.88F,
                      .contrast = 1.20F,
                      .gamma = 1.05F,
                      .gain = 1.0F,
                      .offset = 0.0F,
                      .lift = 0.045F},
    /* Tonemapper: Filmic with lifted toe, compressed shoulder */
    .tonemapper = {.slope = 0.88F,
                   .toe = 0.18F,
                   .shoulder = 0.45F,
                   .black_clip = 0.0F,
                   .white_clip = 0.01F},
    /* Bloom: Atmospheric glow (Soft & Dynamic) */
    .bloom = {.intensity = 0.09F,
              .threshold = 0.80F,
              .soft_threshold = 0.55F,
              .radius = 2.4F},
    /* DoF: Disabled (fog handles depth attenuation) */
    .dof = {.focal_distance = DEFAULT_DOF_FOCAL_DISTANCE,
            .focal_range = DEFAULT_DOF_FOCAL_RANGE,
            .bokeh_scale = DEFAULT_DOF_BOKEH_SCALE},
    /* FXAA: Low sub-pixel to keep grain sharp */
    .fxaa = {.subpix = 0.45F,
             .edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD,
             .edge_threshold_min = DEFAULT_FXAA_EDGE_THRESHOLD_MIN},
    .banding = {.mode = BANDING_MODE_LINEAR,
                .levels = DEFAULT_BANDING_LEVELS,
                .dither_strength = 0.0F,
                .perceptual_gamma = 1.0F,
                .channel_levels = {DEFAULT_BANDING_LEVELS,
                                   DEFAULT_BANDING_LEVELS,
                                   DEFAULT_BANDING_LEVELS}},
    /* Fog: More visible deep cold haze */
    .fog = {.density = 0.022F,
            .start = 3.5F,
            .height_falloff = 0.018F,
            .color = {0.10F, 0.16F, 0.22F}}};

/**
 * @brief Sony Alpha 7S III: Professional cinematic look (S-Cinetone/S-Log3
 * style).
 *
 * Characteristics: Natural mid-tones, soft highlight roll-off, clean and
 * fine organic grain, and breathable shadows.
 */
static const PostProcessPreset PRESET_SONY_A7SIII = {
    .active_effects =
        (unsigned int)POSTFX_VIGNETTE | (unsigned int)POSTFX_GRAIN |
        (unsigned int)POSTFX_AUTO_EXPOSURE | (unsigned int)POSTFX_BLOOM |
        (unsigned int)POSTFX_COLOR_GRADING | (unsigned int)POSTFX_FXAA |
        (unsigned int)POSTFX_DOF | (unsigned int)POSTFX_LUT3D,
    /* Vignette: Subtle and natural */
    .vignette = {.intensity = 0.35F, .smoothness = 0.65F, .roundness = 0.8F},
    /* Grain: Extremely fine, organic 35mm feel */
    .grain = {.intensity = 0.018F,
              .intensity_shadows = 1.1F,
              .intensity_midtones = 1.0F,
              .intensity_highlights = 0.5F,
              .shadows_max = 0.1F,
              .highlights_min = 0.55F,
              .texel_size = 1.2F},
    /* Exposure: Calibrated for middle gray (18%) */
    .exposure = {.exposure = 1.0F},
    /* White Balance: Neutral D65 daylight */
    .white_balance = {.temperature = 6500.0F, .tint = 0.0F},
    /* Color Grading: Vibrant but natural, lifted shadows */
    .color_grading = {.saturation = 1.04F,
                      .contrast = 1.02F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F,
                      .lift = 0.015F},
    /* Tonemapper: S-Cinetone inspired soft roll-off */
    .tonemapper = {.slope = 0.92F,
                   .toe = 0.12F,
                   .shoulder = 0.38F,
                   .black_clip = 0.0F,
                   .white_clip = 0.005F},
    /* Bloom: Subtle highlight diffusion */
    .bloom = {.intensity = 0.035F,
              .threshold = 0.85F,
              .soft_threshold = 0.5F,
              .radius = 1.8F},
    /* Depth of Field: Simulating 35/50mm f/1.8 on FF */
    .dof = {.focal_distance = 16.0F, .focal_range = 4.0F, .bokeh_scale = 12.0F},
    .fxaa = {.subpix = 0.75F,
             .edge_threshold = 0.125F,
             .edge_threshold_min = 0.0625F},
    .banding = {.mode = BANDING_MODE_LINEAR,
                .levels = 256.0F,
                .dither_strength = 0.0F,
                .perceptual_gamma = 1.0F,
                .channel_levels = {256.0F, 256.0F, 256.0F}},
    .fog = {.density = 0.0F,
            .start = 10.0F,
            .height_falloff = 0.1F,
            .color = {0.5F, 0.6F, 0.7F}},
    /* 3D LUT (Gamut Mapping) */
    .lut3d = {.intensity = 1.0F, .texture = 0, .size = 0}};

#endif /* POSTPROCESS_PRESETS_H */
