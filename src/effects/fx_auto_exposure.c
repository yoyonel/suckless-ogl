#include "fx_auto_exposure.h"

#include "gl_common.h"
#include "log.h"
#include "postprocess.h"
#include "shader.h"
#include <stddef.h>

/* Auto Exposure Constants */
enum { LUM_DOWNSAMPLE_SIZE = 64 };
static const float EXPOSURE_INITIAL_VAL = 1.20F;

int fx_auto_exposure_init(PostProcess* post_processing)
{
	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	/* 1. Downsample Logic (64x64 R16F) */
	glGenFramebuffers(1, &auto_exp->downsample_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, auto_exp->downsample_fbo);

	glGenTextures(1, &auto_exp->downsample_tex);
	glBindTexture(GL_TEXTURE_2D, auto_exp->downsample_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, LUM_DOWNSAMPLE_SIZE,
	             LUM_DOWNSAMPLE_SIZE, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, auto_exp->downsample_tex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("suckless-ogl.postprocess.ae",
		          "Lum Downsample FBO incomplete!");
		return 0;
	}

	/* 2. Adaptation Storage (1x1 RGBA32F) */
	glGenTextures(1, &auto_exp->exposure_tex);
	glBindTexture(GL_TEXTURE_2D, auto_exp->exposure_tex);

	float initialValues[4] = {EXPOSURE_INITIAL_VAL, 0.0F, 0.0F, 1.0F};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT,
	             initialValues);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	/* 3. Load Shaders */
	auto_exp->downsample_shader = shader_load(
	    "shaders/postprocess.vert", "shaders/lum_downsample.frag");
	auto_exp->adapt_shader =
	    shader_load_compute_program("shaders/lum_adapt.comp");

	if (!auto_exp->downsample_shader || !auto_exp->adapt_shader) {
		LOG_ERROR("suckless-ogl.postprocess.ae",
		          "Failed to load auto-exposure shaders");
		return 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
	if (auto_exp->downsample_shader) {
		shader_destroy(auto_exp->downsample_shader);
		auto_exp->downsample_shader = NULL;
	}
	if (auto_exp->adapt_shader) {
		shader_destroy(auto_exp->adapt_shader);
		auto_exp->adapt_shader = NULL;
	}
}

void fx_auto_exposure_render(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		return;
	}

	AutoExposureFX* auto_exp = &post_processing->auto_exposure_fx;

	/* 1. Downsample Scene -> 64x64 Log Luminance */
	glViewport(0, 0, LUM_DOWNSAMPLE_SIZE, LUM_DOWNSAMPLE_SIZE);
	glBindFramebuffer(GL_FRAMEBUFFER, auto_exp->downsample_fbo);

	shader_use(auto_exp->downsample_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	shader_set_int(auto_exp->downsample_shader, "sceneTexture", 0);

	glBindVertexArray(post_processing->screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	glBindVertexArray(0);

	/* 2. Compute Adaptation */
	shader_use(auto_exp->adapt_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, auto_exp->downsample_tex);
	shader_set_int(auto_exp->adapt_shader, "lumTexture", 0);

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

	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
	glDispatchCompute(1, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
	                GL_TEXTURE_FETCH_BARRIER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, post_processing->width, post_processing->height);
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
