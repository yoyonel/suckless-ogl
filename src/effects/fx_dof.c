#include "fx_dof.h"

#include "effects/fx_bloom.h"
#include "gl_common.h"
#include "log.h"
#include "postprocess.h"
#include "shader.h"
#include <cglm/types.h>
#include <stddef.h>

int fx_dof_init(PostProcess* post_processing)
{
	DoFFX* dof = &post_processing->dof_fx;

	glGenFramebuffers(1, &dof->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, dof->fbo);

	if (!fx_dof_resize(post_processing)) {
		return 0;
	}

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("suckless-ogl.postprocess.dof",
		          "Failed to create DoF framebuffer");
		return 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 1;
}

void fx_dof_cleanup(PostProcess* post_processing)
{
	DoFFX* dof = &post_processing->dof_fx;

	if (dof->fbo) {
		glDeleteFramebuffers(1, &dof->fbo);
		dof->fbo = 0;
	}
	if (dof->blur_tex) {
		glDeleteTextures(1, &dof->blur_tex);
		dof->blur_tex = 0;
	}
	if (dof->temp_tex) {
		glDeleteTextures(1, &dof->temp_tex);
		dof->temp_tex = 0;
	}
}

int fx_dof_resize(PostProcess* post_processing)
{
	DoFFX* dof = &post_processing->dof_fx;

	int dof_width = post_processing->width / 4;
	int dof_height = post_processing->height / 4;
	if (dof_width < 1) {
		dof_width = 1;
	}
	if (dof_height < 1) {
		dof_height = 1;
	}

	/* Create/Resize Texture (R11F_G11F_B10F is sufficient for bokeh) */
	if (dof->blur_tex) {
		glDeleteTextures(1, &dof->blur_tex);
	}
	glGenTextures(1, &dof->blur_tex);
	glBindTexture(GL_TEXTURE_2D, dof->blur_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, dof_width, dof_height,
	             0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	/* Create/Resize Temp Texture for Ping-Pong */
	if (dof->temp_tex) {
		glDeleteTextures(1, &dof->temp_tex);
	}
	glGenTextures(1, &dof->temp_tex);
	glBindTexture(GL_TEXTURE_2D, dof->temp_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, dof_width, dof_height,
	             0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, dof->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, dof->blur_tex, 0);

	return 1;
}

void fx_dof_render(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_DOF) &&
	    !postprocess_is_enabled(post_processing, POSTFX_DOF_DEBUG)) {
		return;
	}

	DoFFX* dof = &post_processing->dof_fx;
	int dof_width = post_processing->width / 4;
	int dof_height = post_processing->height / 4;
	if (dof_width < 1) {
		dof_width = 1;
	}
	if (dof_height < 1) {
		dof_height = 1;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, dof->fbo);
	glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, dof_width, dof_height);

	/* Pass 1: Downsample/Blur Scene -> Temp (13-tap) */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, dof->temp_tex, 0);

	Shader* ds_shader = fx_bloom_get_downsample_shader(post_processing);
	shader_use(ds_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	shader_set_int(ds_shader, "srcTexture", 0);

	vec2 src_res = {(float)post_processing->width,
	                (float)post_processing->height};
	shader_set_vec2(ds_shader, "srcResolution", (float*)&src_res);

	glBindVertexArray(post_processing->screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	/* Pass 2: Extra Blur (Tent Filter) Temp -> Blur (Final) */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, dof->blur_tex, 0);

	Shader* us_shader = fx_bloom_get_upsample_shader(post_processing);
	shader_use(us_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, dof->temp_tex);
	shader_set_int(us_shader, "srcTexture", 0);
	shader_set_float(us_shader, "filterRadius", 1.0F);

	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, post_processing->width, post_processing->height);
}

void fx_dof_upload_params(Shader* shader, const DoFParams* params)
{
	shader_set_float(shader, "dof.focalDistance", params->focal_distance);
	shader_set_float(shader, "dof.focalRange", params->focal_range);
	shader_set_float(shader, "dof.bokehScale", params->bokeh_scale);
}
