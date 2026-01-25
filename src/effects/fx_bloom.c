#include "fx_bloom.h"

#include "gl_common.h"
#include "log.h"
#include "postprocess.h"
#include "shader.h"
#include <cglm/types.h>
#include <stddef.h>

int fx_bloom_init(PostProcess* post_processing)
{
	BloomFX* bloom = &post_processing->bloom_fx;

	/* Load Shaders */
	bloom->prefilter_shader = shader_load("shaders/postprocess.vert",
	                                      "shaders/bloom_prefilter.frag");
	bloom->downsample_shader = shader_load("shaders/postprocess.vert",
	                                       "shaders/bloom_downsample.frag");
	bloom->upsample_shader = shader_load("shaders/postprocess.vert",
	                                     "shaders/bloom_upsample.frag");

	if (!bloom->prefilter_shader || !bloom->downsample_shader ||
	    !bloom->upsample_shader) {
		LOG_ERROR("suckless-ogl.postprocess.bloom",
		          "Failed to load bloom shaders");
		return 0;
	}

	/* Create Resources */
	glGenFramebuffers(1, &bloom->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, bloom->fbo);

	int width = post_processing->width;
	int height = post_processing->height;

	for (int i = 0; i < BLOOM_MIP_LEVELS; i++) {
		width /= 2;
		height /= 2;
		if (width < 1) {
			width = 1;
		}
		if (height < 1) {
			height = 1;
		}

		bloom->mips[i].width = width;
		bloom->mips[i].height = height;

		glGenTextures(1, &bloom->mips[i].texture);
		glBindTexture(GL_TEXTURE_2D, bloom->mips[i].texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, width, height,
		             0, GL_RGB, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		                GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		                GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
		                GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
		                GL_CLAMP_TO_EDGE);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 1;
}

void fx_bloom_cleanup(PostProcess* post_processing)
{
	BloomFX* bloom = &post_processing->bloom_fx;

	if (bloom->fbo) {
		glDeleteFramebuffers(1, &bloom->fbo);
		bloom->fbo = 0;
	}

	for (int i = 0; i < BLOOM_MIP_LEVELS; i++) {
		if (bloom->mips[i].texture) {
			glDeleteTextures(1, &bloom->mips[i].texture);
			bloom->mips[i].texture = 0;
		}
	}

	if (bloom->prefilter_shader) {
		shader_destroy(bloom->prefilter_shader);
		bloom->prefilter_shader = NULL;
	}
	if (bloom->downsample_shader) {
		shader_destroy(bloom->downsample_shader);
		bloom->downsample_shader = NULL;
	}
	if (bloom->upsample_shader) {
		shader_destroy(bloom->upsample_shader);
		bloom->upsample_shader = NULL;
	}
}

void fx_bloom_render(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		return;
	}

	BloomFX* bloom = &post_processing->bloom_fx;
	glBindFramebuffer(GL_FRAMEBUFFER, bloom->fbo);
	glDisable(GL_DEPTH_TEST);

	/* 1. Prefilter */
	shader_use(bloom->prefilter_shader);
	shader_set_float(bloom->prefilter_shader, "threshold",
	                 post_processing->bloom.threshold);
	shader_set_float(bloom->prefilter_shader, "knee",
	                 post_processing->bloom.soft_threshold);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	shader_set_int(bloom->prefilter_shader, "srcTexture", 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, bloom->mips[0].texture, 0);
	glViewport(0, 0, bloom->mips[0].width, bloom->mips[0].height);

	glBindVertexArray(post_processing->screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	/* 2. Downsample */
	shader_use(bloom->downsample_shader);
	shader_set_int(bloom->downsample_shader, "srcTexture", 0);

	for (int i = 0; i < BLOOM_MIP_LEVELS - 1; i++) {
		const BloomMip* mip_src = &bloom->mips[i];
		const BloomMip* mip_dst = &bloom->mips[i + 1];

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mip_src->texture);

		vec2 resolution = {(float)mip_src->width,
		                   (float)mip_src->height};
		shader_set_vec2(bloom->downsample_shader, "srcResolution",
		                resolution);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, mip_dst->texture, 0);
		glViewport(0, 0, mip_dst->width, mip_dst->height);

		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	}

	/* 3. Upsample with Blending */
	shader_use(bloom->upsample_shader);
	shader_set_int(bloom->upsample_shader, "srcTexture", 0);
	shader_set_float(bloom->upsample_shader, "filterRadius",
	                 post_processing->bloom.radius);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);

	for (int i = BLOOM_MIP_LEVELS - 2; i >= 0; i--) {
		const BloomMip* mip_src = &bloom->mips[i + 1];
		const BloomMip* mip_dst = &bloom->mips[i];

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mip_src->texture);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, mip_dst->texture, 0);
		glViewport(0, 0, mip_dst->width, mip_dst->height);

		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	}

	glDisable(GL_BLEND);
	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, post_processing->width, post_processing->height);
}

void fx_bloom_upload_params(Shader* shader, const BloomParams* params)
{
	shader_set_float(shader, "bloom.intensity", params->intensity);
}

Shader* fx_bloom_get_downsample_shader(PostProcess* post_processing)
{
	return post_processing->bloom_fx.downsample_shader;
}

Shader* fx_bloom_get_upsample_shader(PostProcess* post_processing)
{
	return post_processing->bloom_fx.upsample_shader;
}
