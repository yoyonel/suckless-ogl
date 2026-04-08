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

static const char* const AE_PATH_NAMES[AE_PATH_COUNT] = {"Fragment", "Compute"};

int fx_auto_exposure_init(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	/* 1. Downsample texture — R32F for compute image access, also
	 * compatible with fragment path via FBO attachment. */
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

	/* 2. Fragment path: FBO for fullscreen-quad rendering */
	glGenFramebuffers(1, &auto_exp->downsample_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, auto_exp->downsample_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, auto_exp->downsample_tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("suckless-ogl.postprocess.ae",
		          "Lum Downsample FBO incomplete!");
		fx_auto_exposure_cleanup(post_processing);
		return 0;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	/* 3. Adaptation Storage (1x1 RGBA32F) */
	float initial_values[4] = {EXPOSURE_INITIAL_VAL, 0.0F, 0.0F, 1.0F};
	FXTextureConfig exp_config = {.width = 1,
	                              .height = 1,
	                              .internal_format = GL_RGBA32F,
	                              .format = GL_RGBA,
	                              .type = GL_FLOAT,
	                              .min_filter = GL_NEAREST,
	                              .mag_filter = GL_NEAREST,
	                              .wrap_s = GL_CLAMP_TO_EDGE,
	                              .wrap_t = GL_CLAMP_TO_EDGE,
	                              .initial_data = initial_values};
	fx_utils_create_texture(&auto_exp->exposure_tex, &exp_config);

	/* 4. Load Shaders — both downsample paths + shared adaptation */
	auto_exp->downsample_frag_shader = shader_load(
	    "shaders/postprocess.vert", "shaders/lum_downsample.frag");
	auto_exp->downsample_comp_shader =
	    shader_load_compute_program("shaders/lum_downsample.comp");
	auto_exp->adapt_shader =
	    shader_load_compute_program("shaders/lum_adapt.comp");

	if (!auto_exp->downsample_frag_shader ||
	    !auto_exp->downsample_comp_shader || !auto_exp->adapt_shader) {
		LOG_ERROR("suckless-ogl.postprocess.ae",
		          "Failed to load auto-exposure shaders");
		fx_auto_exposure_cleanup(post_processing);
		return 0;
	}

	/* Set sampler uniforms once */
	shader_use(auto_exp->downsample_frag_shader);
	shader_set_int(auto_exp->downsample_frag_shader, "sceneTexture", 0);
	shader_use(auto_exp->downsample_comp_shader);
	shader_set_int(auto_exp->downsample_comp_shader, "sceneTexture", 0);
	shader_use(auto_exp->adapt_shader);
	shader_set_int(auto_exp->adapt_shader, "lumTexture", 0);

	/* Default to fragment path (proven perf on iGPU / older dGPU) */
	auto_exp->active_path = AE_PATH_FRAGMENT;

	return 1;
}

void fx_auto_exposure_cleanup(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	if (auto_exp->downsample_fbo) {
		glDeleteFramebuffers(1, &auto_exp->downsample_fbo);
		auto_exp->downsample_fbo = 0;
	}
	if (auto_exp->downsample_tex) {
		glDeleteTextures(1, &auto_exp->downsample_tex);
		auto_exp->downsample_tex = 0;
	}
	if (auto_exp->exposure_tex) {
		glDeleteTextures(1, &auto_exp->exposure_tex);
		auto_exp->exposure_tex = 0;
	}
	SHADER_SAFE_DESTROY(auto_exp->downsample_frag_shader);
	SHADER_SAFE_DESTROY(auto_exp->downsample_comp_shader);
	SHADER_SAFE_DESTROY(auto_exp->adapt_shader);
}

static void downsample_fragment(AutoExposureFX* auto_exp,
                                PostProcess* post_processing)
{
	glViewport(0, 0, LUM_HISTOGRAM_MAP_SIZE, LUM_HISTOGRAM_MAP_SIZE);
	glBindFramebuffer(GL_FRAMEBUFFER, auto_exp->downsample_fbo);

	shader_use(auto_exp->downsample_frag_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);

	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, post_processing->width, post_processing->height);
}

static void downsample_compute(AutoExposureFX* auto_exp,
                               PostProcess* post_processing)
{
	shader_use(auto_exp->downsample_comp_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);

	glBindImageTexture(1, auto_exp->downsample_tex, 0, GL_FALSE, 0,
	                   GL_WRITE_ONLY, GL_R32F);

	glDispatchCompute(LUM_HISTOGRAM_MAP_SIZE / LUM_DOWNSAMPLE_GROUP_SIZE,
	                  LUM_HISTOGRAM_MAP_SIZE / LUM_DOWNSAMPLE_GROUP_SIZE,
	                  1);

	glMemoryBarrier((GLbitfield)GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
	                (GLbitfield)GL_TEXTURE_FETCH_BARRIER_BIT);
}

void fx_auto_exposure_render(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		return;
	}

	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	/* 1. Downsample Scene -> 64x64 Log Luminance */
	if (auto_exp->active_path == AE_PATH_COMPUTE) {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler,
		                   "Auto-Exposure Downsample (Compute)",
		                   GPU_PROFILER_AUTO_EXPOSURE_COLOR);
		downsample_compute(auto_exp, post_processing);
	} else {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler,
		                   "Auto-Exposure Downsample (Fragment)",
		                   GPU_PROFILER_AUTO_EXPOSURE_COLOR);
		downsample_fragment(auto_exp, post_processing);
	}

	/* 2. Compute Adaptation (always compute — single invocation) */
	{
		GPU_STAGE_PROFILER(post_processing->gpu_profiler,
		                   "Auto-Exposure (Adaptation)",
		                   GPU_PROFILER_AUTO_EXPOSURE_COLOR);

		/* Fragment path needs a texture fetch barrier before compute
		 * reads the FBO output as a sampled texture */
		if (auto_exp->active_path == AE_PATH_FRAGMENT) {
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		}

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

void fx_auto_exposure_toggle_path(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;
	auto_exp->active_path =
	    (AEDownsamplePath)((auto_exp->active_path + 1) % AE_PATH_COUNT);
	LOG_INFO("suckless-ogl.postprocess.ae",
	         "Auto-Exposure downsample path: %s",
	         AE_PATH_NAMES[auto_exp->active_path]);
}

const char* fx_auto_exposure_path_name(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;
	return AE_PATH_NAMES[auto_exp->active_path];
}
