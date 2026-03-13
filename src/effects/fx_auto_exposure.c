#include "effects/fx_auto_exposure.h"

#include "app_settings.h"
#include "effects/fx_utils.h"
#include "gl_common.h"
#include "log.h"
#include "postprocess.h"
#include "shader.h"
#include <stddef.h>

/* Auto Exposure Constants */
static const float EXPOSURE_INITIAL_VAL = 1.20F;

int fx_auto_exposure_init(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	/* 1. Downsample Texture Storage (64x64 R16F)
	 * This texture is now written by the Bloom SPD compute shader
	 * but owned by AE for adaptation. */
	FXTextureConfig ds_config = {.width = LUM_HISTOGRAM_MAP_SIZE,
	                             .height = LUM_HISTOGRAM_MAP_SIZE,
	                             .internal_format = GL_R32F,
	                             .format = GL_RED,
	                             .type = GL_FLOAT,
	                             .min_filter = GL_LINEAR,
	                             .mag_filter = GL_LINEAR,
	                             .wrap_s = GL_CLAMP_TO_EDGE,
	                             .wrap_t = GL_CLAMP_TO_EDGE,
	                             .initial_data = NULL};
	fx_utils_create_texture(&auto_exp->downsample_tex, &ds_config);

	/* 2. Adaptation Storage (1x1 RGBA32F) */
	float initialValues[4] = {EXPOSURE_INITIAL_VAL, 0.0F, 0.0F, 1.0F};
	FXTextureConfig exp_config = {.width = 1,
	                              .height = 1,
	                              .internal_format = GL_RGBA32F,
	                              .format = GL_RGBA,
	                              .type = GL_FLOAT,
	                              .min_filter = GL_NEAREST,
	                              .mag_filter = GL_NEAREST,
	                              .wrap_s = GL_CLAMP_TO_EDGE,
	                              .wrap_t = GL_CLAMP_TO_EDGE,
	                              .initial_data = initialValues};
	fx_utils_create_texture(&auto_exp->exposure_tex, &exp_config);

	/* 3. Load Shaders */
	auto_exp->adapt_shader =
	    shader_load_compute_program("shaders/lum_adapt.comp");

	if (!auto_exp->adapt_shader) {
		LOG_ERROR("suckless-ogl.postprocess.ae",
		          "Failed to load auto-exposure adaptation shader");
		fx_auto_exposure_cleanup(post_processing);
		return 0;
	}

	/* Set sampler uniforms once */
	shader_use(auto_exp->adapt_shader);
	shader_set_int(auto_exp->adapt_shader, "lumTexture", 0);

	return 1;
}

void fx_auto_exposure_cleanup(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	if (auto_exp->downsample_tex) {
		glDeleteTextures(1, &auto_exp->downsample_tex);
		auto_exp->downsample_tex = 0;
	}
	if (auto_exp->exposure_tex) {
		glDeleteTextures(1, &auto_exp->exposure_tex);
		auto_exp->exposure_tex = 0;
	}
	SHADER_SAFE_DESTROY(auto_exp->adapt_shader);
}

void fx_auto_exposure_render(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		return;
	}

	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	/* 1. Downsample Logic (REMOVED)
	 * Luminance is now calculated by Bloom SPD Compute shader to save
	 * bandwidth. We directly use auto_exp->downsample_tex which was
	 * populated by fx_bloom_render. */

	/* 2. Compute Adaptation */
	shader_use(auto_exp->adapt_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, auto_exp->downsample_tex);

	glBindImageTexture(1, auto_exp->exposure_tex, 0, GL_FALSE, 0,
	                   GL_READ_WRITE, GL_RGBA32F);

	shader_set_float(auto_exp->adapt_shader, "deltaTime",
	                 post_processing->delta_time);
	shader_set_float(auto_exp->adapt_shader, "minLuminance",
	                 post_processing->auto_exposure.min_luminance);
	shader_set_float(auto_exp->adapt_shader, "maxLuminance",
	                 post_processing->auto_exposure.max_luminance);
	shader_set_float(auto_exp->adapt_shader, "speedUp",
	                 post_processing->auto_exposure.speed_up);
	shader_set_float(auto_exp->adapt_shader, "speedDown",
	                 post_processing->auto_exposure.speed_down);
	shader_set_float(auto_exp->adapt_shader, "keyValue",
	                 post_processing->auto_exposure.key_value);

	/* Unbind any FB just in case, though we only use compute here */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	/* Ensure Bloom SPD image writes to downsample_tex are visible to
	 * texture fetch */
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

	glDispatchCompute(1, 1, 1);

	/* Final barrier for exposure_tex to be visible to final composite */
	glMemoryBarrier((GLbitfield)GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
	                (GLbitfield)GL_TEXTURE_FETCH_BARRIER_BIT);
}

float fx_auto_exposure_get_current_exposure(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;
	float pixel[4];
	glBindTexture(GL_TEXTURE_2D, auto_exp->exposure_tex);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixel);
	glBindTexture(GL_TEXTURE_2D, 0);
	return pixel[0];
}
