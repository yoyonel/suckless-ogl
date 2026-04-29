#include "gl_common.h"
#include "log.h"
#include "postprocess.h"

static void postprocess_on_state_change(PostProcess* post_processing)
{
	/* Clear stale sync objects if effect was disabled to prevent stalls
	 * when re-enabling */
	if (!postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		for (int i = 0; i < 2; i++) {
			if (post_processing->readback.exposure_sync[i]) {
				glDeleteSync(
				    post_processing->readback.exposure_sync[i]);
				post_processing->readback.exposure_sync[i] =
				    NULL;
			}
		}
	}
	if (!postprocess_is_enabled(post_processing, POSTFX_EXPOSURE_DEBUG)) {
		for (int i = 0; i < 2; i++) {
			if (post_processing->readback.histogram_sync[i]) {
				glDeleteSync(post_processing->readback
				                 .histogram_sync[i]);
				post_processing->readback.histogram_sync[i] =
				    NULL;
			}
		}
	}

	post_processing->ubo_dirty = true;
	if (post_processing->shaders.is_optimized) {
		if (post_processing->active_effects ==
		    post_processing->shaders.compiled_flags) {
			return;
		}
		LOG_INFO(
		    "suckless-ogl.postprocess",
		    "State changed in optimized mode - recompiling shader...");
		postprocess_compile_optimized(post_processing,
		                              post_processing->active_effects);
	}
}

void postprocess_enable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects |= (unsigned int)effect;
	postprocess_on_state_change(post_processing);
}

void postprocess_disable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects &= ~(unsigned int)effect;
	postprocess_on_state_change(post_processing);
}

void postprocess_toggle(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects ^= (unsigned int)effect;
	postprocess_on_state_change(post_processing);
}

int postprocess_is_enabled(PostProcess* post_processing,
                           PostProcessEffect effect)
{
	return (post_processing->active_effects & (unsigned int)effect) != 0;
}

void postprocess_set_vignette(PostProcess* post_processing, float intensity,
                              float smoothness, float roundness)
{
	post_processing->vignette.intensity = intensity;
	post_processing->vignette.smoothness = smoothness;
	post_processing->vignette.roundness = roundness;
	post_processing->ubo_dirty = true;
}

void postprocess_set_grain(PostProcess* post_processing, float intensity)
{
	post_processing->grain.intensity = intensity;
	post_processing->ubo_dirty = true;
}

void postprocess_set_exposure(PostProcess* post_processing, float exposure)
{
	post_processing->exposure.exposure = exposure;
	post_processing->ubo_dirty = true;
}

void postprocess_set_chrom_abbr(PostProcess* post_processing, float strength)
{
	post_processing->chrom_abbr.strength = strength;
	post_processing->ubo_dirty = true;
}

void postprocess_set_white_balance(PostProcess* post_processing,
                                   float temperature, float tint)
{
	post_processing->white_balance.temperature = temperature;
	post_processing->white_balance.tint = tint;
	post_processing->ubo_dirty = true;
}

void postprocess_set_color_grading(PostProcess* post_processing,
                                   float saturation, float contrast,
                                   float gamma, float gain, float offset,
                                   float lift)
{
	post_processing->color_grading.saturation = saturation;
	post_processing->color_grading.contrast = contrast;
	post_processing->color_grading.gamma = gamma;
	post_processing->color_grading.gain = gain;
	post_processing->color_grading.offset = offset;
	post_processing->color_grading.lift = lift;
	post_processing->ubo_dirty = true;
}

void postprocess_set_tonemapper(PostProcess* post_processing, float slope,
                                float toe, float shoulder, float black_clip,
                                float white_clip)
{
	post_processing->tonemapper.slope = slope;
	post_processing->tonemapper.toe = toe;
	post_processing->tonemapper.shoulder = shoulder;
	post_processing->tonemapper.black_clip = black_clip;
	post_processing->tonemapper.white_clip = white_clip;
	post_processing->ubo_dirty = true;
}

void postprocess_set_bloom(PostProcess* post_processing, float intensity,
                           float threshold, float soft_threshold)
{
	post_processing->bloom.intensity = intensity;
	post_processing->bloom.threshold = threshold;
	post_processing->bloom.soft_threshold = soft_threshold;
	post_processing->ubo_dirty = true;
}

void postprocess_set_dof(PostProcess* post_processing, float focal_distance,
                         float focal_range, float bokeh_scale)
{
	post_processing->dof.focal_distance = focal_distance;
	post_processing->dof.focal_range = focal_range;
	post_processing->dof.bokeh_scale = bokeh_scale;
	/* Preserve anamorphic ratio if not specified in this helper */
	post_processing->ubo_dirty = true;
}

void postprocess_set_dof_anamorphic(PostProcess* post_processing,
                                    float anamorphic_ratio)
{
	post_processing->dof.anamorphic_ratio = anamorphic_ratio;
	post_processing->ubo_dirty = true;
}

float postprocess_get_exposure(PostProcess* post_processing)
{
	/* Return the cached value from the last async readback to avoid
	 * synchronous GPU stalls. If AE is disabled, return manual exposure. */
	if (postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		return post_processing->readback.current_exposure;
	}
	return post_processing->exposure.exposure;
}

void postprocess_set_auto_exposure(PostProcess* post_processing,
                                   float min_luminance, float max_luminance,
                                   float speed_up, float speed_down,
                                   float key_value)
{
	post_processing->auto_exposure.min_luminance = min_luminance;
	post_processing->auto_exposure.max_luminance = max_luminance;
	post_processing->auto_exposure.speed_up = speed_up;
	post_processing->auto_exposure.speed_down = speed_down;
	post_processing->auto_exposure.key_value = key_value;
	post_processing->ubo_dirty = true;
}

void postprocess_set_fxaa(PostProcess* post_processing, float subpix,
                          float edge_threshold, float edge_threshold_min)
{
	post_processing->fxaa.subpix = subpix;
	post_processing->fxaa.edge_threshold = edge_threshold;
	post_processing->fxaa.edge_threshold_min = edge_threshold_min;
	post_processing->ubo_dirty = true;
}

void postprocess_set_banding(PostProcess* post_processing, BandingMode mode,
                             float levels)
{
	post_processing->banding.mode = (int32_t)mode;
	post_processing->banding.levels = levels;
	post_processing->ubo_dirty = true;
}

void postprocess_set_banding_dither(PostProcess* post_processing,
                                    float strength)
{
	post_processing->banding.dither_strength = strength;
	post_processing->ubo_dirty = true;
}

void postprocess_set_banding_perceptual(PostProcess* post_processing,
                                        float gamma)
{
	post_processing->banding.perceptual_gamma = gamma;
	post_processing->ubo_dirty = true;
}

void postprocess_set_banding_channels(PostProcess* post_processing, float red,
                                      float green, float blue)
{
	post_processing->banding.channel_levels[0] = red;
	post_processing->banding.channel_levels[1] = green;
	post_processing->banding.channel_levels[2] = blue;
	post_processing->ubo_dirty = true;
}

void postprocess_set_fog(PostProcess* post_processing, float density,
                         float start, float height_falloff, float fog_r,
                         float fog_g, float fog_b)
{
	post_processing->fog.density = density;
	post_processing->fog.start = start;
	post_processing->fog.height_falloff = height_falloff;
	post_processing->fog.color[0] = fog_r;
	post_processing->fog.color[1] = fog_g;
	post_processing->fog.color[2] = fog_b;
	post_processing->ubo_dirty = true;
}

void postprocess_set_grading_ue_default(PostProcess* post_processing)
{
	/* * Valeurs par défaut d'Unreal Engine (Section "Global").
	 * Le "look" UE vient de l'application de ces valeurs neutres
	 * combinées à la courbe de tone mapping ACES dans le shader.
	 */
	post_processing->color_grading.saturation = 1.0F;
	post_processing->color_grading.contrast = 1.0F;
	post_processing->color_grading.gamma = 1.0F;
	post_processing->color_grading.gain = 1.0F;
	post_processing->color_grading.offset = 0.0F;
	post_processing->ubo_dirty = true;

	/* On s'assure que l'effet est activé pour passer dans le pipeline ACES
	 */
	postprocess_enable(post_processing, POSTFX_COLOR_GRADING);
}

void postprocess_apply_preset(PostProcess* post_processing,
                              const PostProcessPreset* preset)
{
	post_processing->active_effects = preset->active_effects;
	post_processing->vignette = preset->vignette;
	post_processing->grain = preset->grain;
	post_processing->exposure = preset->exposure;
	post_processing->chrom_abbr = preset->chrom_abbr;
	post_processing->white_balance = preset->white_balance;
	post_processing->color_grading = preset->color_grading;
	post_processing->tonemapper = preset->tonemapper;
	post_processing->bloom = preset->bloom;
	post_processing->dof = preset->dof;
	post_processing->fxaa = preset->fxaa;
	post_processing->banding = preset->banding;
	post_processing->fog = preset->fog;
	post_processing->lut3d = preset->lut3d;
	post_processing->ubo_dirty = true;

	postprocess_on_state_change(post_processing);
}

void postprocess_set_lut3d(PostProcess* post_processing, float intensity,
                           GLuint texture)
{
	post_processing->lut3d.intensity = intensity;
	post_processing->lut3d.texture = texture;
	post_processing->ubo_dirty = true;
}

int postprocess_load_lut3d(PostProcess* post_processing, const char* path)
{
	return fx_lut3d_load_cube(&post_processing->lut3d_fx,
	                          &post_processing->lut3d, path);
}
