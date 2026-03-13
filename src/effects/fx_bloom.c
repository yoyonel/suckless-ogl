#include "effects/fx_bloom.h"

#include "effects/fx_utils.h"
#include "gl_common.h"
#include "log.h"
#include "postprocess.h"
#include "shader.h"
#include <cglm/types.h>
#include <stddef.h>

int fx_bloom_init(PostProcess* post_processing)
{
	/* Ensure Unit 0 is active for initial texture setup */
	glActiveTexture(GL_TEXTURE0);

	BloomFX* bloom = &post_processing->bloom_fx;

	/* Load Shaders */
	bloom->prefilter_shader = shader_load("shaders/postprocess.vert",
	                                      "shaders/bloom_prefilter.frag");
	bloom->downsample_shader = shader_load("shaders/postprocess.vert",
	                                       "shaders/bloom_downsample.frag");
	bloom->upsample_shader = shader_load("shaders/postprocess.vert",
	                                     "shaders/bloom_upsample.frag");
	bloom->compute_downsample_shader =
	    shader_load_compute_program("shaders/bloom_downsample.comp");

	if (!bloom->prefilter_shader || !bloom->downsample_shader ||
	    !bloom->upsample_shader || !bloom->compute_downsample_shader) {
		LOG_ERROR("suckless-ogl.postprocess.bloom",
		          "Failed to load bloom shaders");
		fx_bloom_cleanup(post_processing);
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

	int width = post_processing->width;
	int height = post_processing->height;

	for (int i = 0; i < BLOOM_MIP_LEVELS; i++) {
		BloomMip* mip = &bloom->mips[i];
		mip->width = width;
		mip->height = height;

		FXTextureConfig config = {.width = width,
		                          .height = height,
		                          .internal_format = GL_R11F_G11F_B10F,
		                          .format = GL_RGB,
		                          .type = GL_FLOAT,
		                          .min_filter = GL_LINEAR,
		                          .mag_filter = GL_LINEAR,
		                          .wrap_s = GL_CLAMP_TO_EDGE,
		                          .wrap_t = GL_CLAMP_TO_EDGE,
		                          .initial_data = NULL};
		fx_utils_create_texture(&mip->texture, &config);

		width /= 2;
		height /= 2;
		if (width < 1) {
			width = 1;
		}
		if (height < 1) {
			height = 1;
		}
	}

	/* Initialize FBO by attaching the first mip (Prefilter target) */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, bloom->mips[0].texture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("suckless-ogl.postprocess.bloom",
		          "Bloom FBO incomplete!");
		fx_bloom_cleanup(post_processing);
		return 0;
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

	SHADER_SAFE_DESTROY(bloom->prefilter_shader);
	SHADER_SAFE_DESTROY(bloom->downsample_shader);
	SHADER_SAFE_DESTROY(bloom->upsample_shader);
	SHADER_SAFE_DESTROY(bloom->compute_downsample_shader);
}

void fx_bloom_render(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		return;
	}

	BloomFX* bloom = &post_processing->bloom_fx;

	/* 1. Prefilter */
	shader_use(bloom->prefilter_shader);
	fx_bloom_upload_params(bloom->prefilter_shader,
	                       &post_processing->bloom);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);

	glBindFramebuffer(GL_FRAMEBUFFER, bloom->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, bloom->mips[0].texture, 0);
	glViewport(0, 0, bloom->mips[0].width, bloom->mips[0].height);

	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	if (post_processing->bloom_fx.debug_step == 1) { /* Prefilter only */
		goto end_bloom;
	}

	/* 2. Compute Downsample (SPD) */
	/* Replaces the old 5-6 fragment passes with a single dispatch */
	shader_use(bloom->compute_downsample_shader);

	/* Bind Mip 0 as source sampler */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, bloom->mips[0].texture);
	shader_set_int(bloom->compute_downsample_shader, "srcTexture", 0);

	vec2 src_res = {(float)bloom->mips[0].width,
	                (float)bloom->mips[0].height};
	shader_set_vec2(bloom->compute_downsample_shader, "srcResolution",
	                src_res);

	/* Bind Mips 1-4 as output images */
	/* Note: Mip 0 is the SOURCE (Prefiltered), Mip 1 is first DOWN */
	for (int i = 1; i < BLOOM_MIP_LEVELS; i++) {
		glBindImageTexture((GLuint)i, bloom->mips[i].texture, 0,
		                   GL_FALSE, 0, GL_WRITE_ONLY,
		                   GL_R11F_G11F_B10F);
	}

	/* Bind Auto-Exposure Downsample Texture (Unit 6) for integrated
	 * luminance */
	glBindImageTexture(BLOOM_BINDING_LUMINANCE,
	                   post_processing->auto_exposure_fx.downsample_tex, 0,
	                   GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

	/* Dispatch: each thread handles a 2x2 area to produce 1 pixel in Mip 1
	 */
	GLuint groups_x =
	    (GLuint)((bloom->mips[1].width + (BLOOM_COMPUTE_GROUP_SIZE - 1)) /
	             BLOOM_COMPUTE_GROUP_SIZE);
	GLuint groups_y =
	    (GLuint)((bloom->mips[1].height + (BLOOM_COMPUTE_GROUP_SIZE - 1)) /
	             BLOOM_COMPUTE_GROUP_SIZE);
	glDispatchCompute(groups_x, groups_y, 1);

	/* Ensure all image writes are finished before upsampling or
	 * compositing */
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	if (post_processing->bloom_fx.debug_step == 2) { /* Downsample only */
		goto end_bloom;
	}

	/* 3. Upsample (Accumulative) */
	shader_use(bloom->upsample_shader);
	shader_set_float(bloom->upsample_shader, "filterRadius",
	                 post_processing->bloom.radius);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	for (int i = BLOOM_MIP_LEVELS - 1; i > 0; i--) {
		const BloomMip* mip_src = &bloom->mips[i];
		const BloomMip* mip_dst = &bloom->mips[i - 1];

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mip_src->texture);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, mip_dst->texture, 0);
		glViewport(0, 0, mip_dst->width, mip_dst->height);

		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

		if (post_processing->bloom_fx.debug_step == 3 &&
		    post_processing->bloom_fx.debug_mip == i) {
			glDisable(GL_BLEND);
			goto end_bloom;
		}
	}

	glDisable(GL_BLEND);

end_bloom:
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void fx_bloom_upload_params(Shader* shader, const BloomParams* params)
{
	shader_set_float(shader, "bloom.intensity", params->intensity);
	shader_set_float(shader, "bloom.threshold", params->threshold);
	shader_set_float(shader, "bloom.soft_threshold",
	                 params->soft_threshold);
}

Shader* fx_bloom_get_downsample_shader(PostProcess* post_processing)
{
	return post_processing->bloom_fx.downsample_shader;
}

Shader* fx_bloom_get_upsample_shader(PostProcess* post_processing)
{
	return post_processing->bloom_fx.upsample_shader;
}
