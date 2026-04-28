#include "effects/fx_bloom.h"

#include "effects/effect_context.h"
#include "effects/fx_utils.h"
#include "gl_common.h"
#include "log.h"
#include "shader.h"
#include <cglm/types.h>
#include <stddef.h>

int fx_bloom_init(BloomFX* bloom, int width, int height)
{
	/* Ensure Unit 0 is active for initial texture setup */
	glActiveTexture(GL_TEXTURE0);

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
		fx_bloom_cleanup(bloom);
		return 0;
	}

	/* Set sampler uniforms once (they are per-program state) */
	shader_use(bloom->prefilter_shader);
	shader_set_int(bloom->prefilter_shader, "srcTexture", 0);
	shader_use(bloom->downsample_shader);
	shader_set_int(bloom->downsample_shader, "srcTexture", 0);
	shader_use(bloom->upsample_shader);
	shader_set_int(bloom->upsample_shader, "srcTexture", 0);

	/* Create Resources */
	glGenFramebuffers(1, &bloom->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, bloom->fbo);

	int mip_w = width;
	int mip_h = height;

	for (int i = 0; i < BLOOM_MIP_LEVELS; i++) {
		mip_w /= 2;
		mip_h /= 2;
		if (mip_w < 1) {
			mip_w = 1;
		}
		if (mip_h < 1) {
			mip_h = 1;
		}

		bloom->mips[i].width = mip_w;
		bloom->mips[i].height = mip_h;

		FXTextureConfig tex_config = {
		    .width = mip_w,
		    .height = mip_h,
		    .internal_format = GL_R11F_G11F_B10F,
		    .format = GL_RGB,
		    .type = GL_FLOAT,
		    .min_filter = GL_LINEAR,
		    .mag_filter = GL_LINEAR,
		    .wrap_s = GL_CLAMP_TO_EDGE,
		    .wrap_t = GL_CLAMP_TO_EDGE,
		    .initial_data = NULL};
		fx_utils_create_texture(&bloom->mips[i].texture, &tex_config);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 1;
}

void fx_bloom_cleanup(BloomFX* bloom)
{
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

	SHADER_SAFE_DESTROY(bloom->prefilter_shader);
	SHADER_SAFE_DESTROY(bloom->downsample_shader);
	SHADER_SAFE_DESTROY(bloom->upsample_shader);
}

void fx_bloom_render(BloomFX* bloom, const BloomParams* params,
                     const EffectContext* ctx)
{
	glBindFramebuffer(GL_FRAMEBUFFER, bloom->fbo);
	glDisable(GL_DEPTH_TEST);

	/* 1. Prefilter */
	shader_use(bloom->prefilter_shader);
	shader_set_float(bloom->prefilter_shader, "threshold",
	                 params->threshold);
	shader_set_float(bloom->prefilter_shader, "knee",
	                 params->soft_threshold);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ctx->src_tex);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, bloom->mips[0].texture, 0);
	glViewport(0, 0, bloom->mips[0].width, bloom->mips[0].height);

	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	if (bloom->debug_step == 1) { /* Prefilter only */
		goto end_bloom;
	}

	/* 2. Downsample */
	shader_use(bloom->downsample_shader);

	for (int i = 0; i < BLOOM_MIP_LEVELS - 1; i++) {
		const BloomMip* mip_src = &bloom->mips[i];
		const BloomMip* mip_dst = &bloom->mips[i + 1];

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mip_src->texture);

		vec2 resolution = {(float)mip_src->width,
		                   (float)mip_src->height};
		shader_set_vec2(bloom->downsample_shader, "srcResolution",
		                resolution);
		vec2 neutralScale = {1.0F, 1.0F};
		shader_set_vec2(bloom->downsample_shader, "texelScale",
		                (float*)&neutralScale);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, mip_dst->texture, 0);
		glViewport(0, 0, mip_dst->width, mip_dst->height);

		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	}

	if (bloom->debug_step == 2) { /* Downsample only */
		goto end_bloom;
	}

	/* 3. Upsample with Blending */
	shader_use(bloom->upsample_shader);
	shader_set_float(bloom->upsample_shader, "filterRadius",
	                 params->radius);
	vec2 neutralScale = {1.0F, 1.0F};
	shader_set_vec2(bloom->upsample_shader, "texelScale",
	                (float*)&neutralScale);

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

end_bloom:
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, ctx->width, ctx->height);
}

void fx_bloom_upload_params(Shader* shader, const BloomParams* params)
{
	shader_set_float(shader, "bloom.intensity", params->intensity);
}

Shader* fx_bloom_get_downsample_shader(BloomFX* bloom)
{
	return bloom->downsample_shader;
}

Shader* fx_bloom_get_upsample_shader(BloomFX* bloom)
{
	return bloom->upsample_shader;
}
