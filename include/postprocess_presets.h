#ifndef POSTPROCESS_PRESETS_H
#define POSTPROCESS_PRESETS_H

#include "postprocess.h"

/*
 * Definitions des presets de post-traitement.
 * Ces constantes sont définies ici pour être accessibles via include
 * sans nécessiter de compilation séparée.
 */

static const PostProcessPreset PRESET_DEFAULT = {
    .active_effects =
        (unsigned int)POSTFX_EXPOSURE | (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = DEFAULT_VIGNETTE_INTENSITY,
                 .extent = DEFAULT_VIGNETTE_EXTENT},
    .grain = {.intensity = DEFAULT_GRAIN_INTENSITY},
    .exposure = {.exposure = DEFAULT_EXPOSURE},
    .chrom_abbr = {.strength = DEFAULT_CHROM_ABBR_STRENGTH},
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F}};

static const PostProcessPreset PRESET_SUBTLE = {
    .active_effects =
        (unsigned int)POSTFX_VIGNETTE | (unsigned int)POSTFX_GRAIN |
        (unsigned int)POSTFX_CHROM_ABBR | (unsigned int)POSTFX_EXPOSURE |
        (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = 0.3F, .extent = 0.7F},
    .grain = {.intensity = 0.02F},
    .exposure = {.exposure = DEFAULT_EXPOSURE},
    .chrom_abbr = {.strength = 0.01F},
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F}};

static const PostProcessPreset PRESET_CINEMATIC = {
    .active_effects =
        (unsigned int)POSTFX_VIGNETTE | (unsigned int)POSTFX_GRAIN |
        (unsigned int)POSTFX_CHROM_ABBR | (unsigned int)POSTFX_EXPOSURE |
        (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = 0.5F, .extent = 0.6F},
    .grain = {.intensity = 0.03F},
    .exposure = {.exposure = 1.2F},
    .chrom_abbr = {.strength = 0.015F},
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F}};

static const PostProcessPreset PRESET_VINTAGE = {
    .active_effects =
        (unsigned int)POSTFX_VIGNETTE | (unsigned int)POSTFX_GRAIN |
        (unsigned int)POSTFX_CHROM_ABBR | (unsigned int)POSTFX_EXPOSURE |
        (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = 0.7F, .extent = 0.5F},
    .grain = {.intensity = 0.06F},
    .exposure = {.exposure = 0.9F},
    .chrom_abbr = {.strength = 0.02F},
    .color_grading = {.saturation = 1.0F,
                      .contrast = 1.0F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F}};

static const PostProcessPreset PRESET_MATRIX = {
    .active_effects = (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = DEFAULT_VIGNETTE_INTENSITY,
                 .extent = DEFAULT_VIGNETTE_EXTENT},
    .grain = {.intensity = DEFAULT_GRAIN_INTENSITY},
    .exposure = {.exposure = DEFAULT_EXPOSURE},
    .chrom_abbr = {.strength = DEFAULT_CHROM_ABBR_STRENGTH},
    .color_grading = {.saturation = 0.5F,
                      .contrast = 1.2F,
                      .gamma = 0.9F,
                      .gain = 1.1F,
                      .offset = 0.02F}};

static const PostProcessPreset PRESET_BW_CONTRAST = {
    .active_effects = (unsigned int)POSTFX_COLOR_GRADING,
    .vignette = {.intensity = DEFAULT_VIGNETTE_INTENSITY,
                 .extent = DEFAULT_VIGNETTE_EXTENT},
    .grain = {.intensity = DEFAULT_GRAIN_INTENSITY},
    .exposure = {.exposure = DEFAULT_EXPOSURE},
    .chrom_abbr = {.strength = DEFAULT_CHROM_ABBR_STRENGTH},
    .color_grading = {.saturation = 0.0F,
                      .contrast = 1.5F,
                      .gamma = 1.0F,
                      .gain = 1.0F,
                      .offset = 0.0F}};

#endif /* POSTPROCESS_PRESETS_H */
