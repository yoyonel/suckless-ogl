/**
 * @file postprocess_setters.h
 * @brief Parameter setter/getter API for the post-processing pipeline.
 *
 * Consumers that only configure effect parameters should include this header
 * instead of the full postprocess.h.
 */

#ifndef POSTPROCESS_SETTERS_H
#define POSTPROCESS_SETTERS_H

#include <glad/glad.h>

#include "pp_params.h"

typedef struct PostProcess PostProcess;

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

#endif /* POSTPROCESS_SETTERS_H */
