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
static const int LUM_DOWNSAMPLE_GROUP_SIZE = 16;

int fx_auto_exposure_init(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	/* 1. Downsample Logic (64x64 R32F for Compute Image Access) */
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

	/* 3. Load Shaders (Both are now Compute Programs) */
	auto_exp->downsample_shader =
	    shader_load_compute_program("shaders/lum_downsample.comp");
	auto_exp->adapt_shader =
	    shader_load_compute_program("shaders/lum_adapt.comp");

	if (!auto_exp->downsample_shader || !auto_exp->adapt_shader) {
		LOG_ERROR("suckless-ogl.postprocess.ae",
		          "Failed to load auto-exposure shaders");
		fx_auto_exposure_cleanup(post_processing);
		return 0;
	}

	/* Set uniforms once */
	shader_use(auto_exp->downsample_shader);
	shader_set_int(auto_exp->downsample_shader, "sceneTexture", 0);
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
	SHADER_SAFE_DESTROY(auto_exp->downsample_shader);
	SHADER_SAFE_DESTROY(auto_exp->adapt_shader);
}

void fx_auto_exposure_render(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		return;
	}

	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	/* 1. Downsample Scene -> 64x64 Log Luminance (Compute) */
	{
		GPU_STAGE_PROFILER(post_processing->gpu_profiler,
		                   "Auto-Exposure (Downsample)",
		                   GPU_PROFILER_AUTO_EXPOSURE_COLOR);

		shader_use(auto_exp->downsample_shader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);

		glBindImageTexture(1, auto_exp->downsample_tex, 0, GL_FALSE, 0,
		                   GL_WRITE_ONLY, GL_R32F);

		/* 8x8 workgroups of 8x8 threads = 64x64 output */
		glDispatchCompute(
		    LUM_HISTOGRAM_MAP_SIZE / LUM_DOWNSAMPLE_GROUP_SIZE,
		    LUM_HISTOGRAM_MAP_SIZE / LUM_DOWNSAMPLE_GROUP_SIZE, 1);

		/* Barrier ensuring that imageStore from downsample is completed
		 * before adaptation reads from it as a texture. */
		glMemoryBarrier((GLbitfield)GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
		                (GLbitfield)GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	/* 2. Compute Adaptation (Compute) */
	{
		GPU_STAGE_PROFILER(post_processing->gpu_profiler,
		                   "Auto-Exposure (Adaptation)",
		                   GPU_PROFILER_AUTO_EXPOSURE_COLOR);

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

		glDispatchCompute(1, 1, 1);

		/* Sync for final exposure texture access in main uber-shader */
		glMemoryBarrier((GLbitfield)GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
		                (GLbitfield)GL_TEXTURE_FETCH_BARRIER_BIT);
	}
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
