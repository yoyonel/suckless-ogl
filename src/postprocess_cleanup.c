#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_lut3d.h"
#include "effects/fx_lut_viz.h"
#include "effects/fx_motion_blur.h"
#include "gl_common.h"
#include "log.h"
#include "postprocess_internal.h"
#include "render_utils.h"

void pp_destroy_framebuffer(PostProcess* post_processing)
{
	GL_SAFE_DELETE_TEXTURE(post_processing->gpu.scene_color_tex);
	GL_SAFE_DELETE_TEXTURE(post_processing->gpu.velocity_tex);
	GL_SAFE_DELETE_TEXTURE(post_processing->gpu.scene_depth_tex);
	GL_SAFE_DELETE_TEXTURE(post_processing->gpu.scene_stencil_view);
	GL_SAFE_DELETE_FRAMEBUFFER(post_processing->gpu.scene_fbo);

	/* Bridge Unit 0 with dummy to avoid invalid state warnings during
	 * resize
	 */
	if (post_processing->gpu.dummy_black_tex) {
		render_utils_bind_texture_safe(
		    GL_TEXTURE0, 0, post_processing->gpu.dummy_black_tex);
	}
}

void pp_destroy_screen_quad(PostProcess* post_processing)
{
	GL_SAFE_DELETE_VAO(post_processing->gpu.screen_quad_vao);
	GL_SAFE_DELETE_BUFFER(post_processing->gpu.screen_quad_vbo);
}

void pp_destroy_readback_buffers(PostProcess* post_processing)
{
	for (int i = 0; i < 2; i++) {
		GL_SAFE_DELETE_BUFFER(
		    post_processing->readback.exposure_pbo[i]);
		GL_SAFE_DELETE_BUFFER(
		    post_processing->readback.histogram_pbo[i]);
		if (post_processing->readback.exposure_sync[i]) {
			glDeleteSync(
			    post_processing->readback.exposure_sync[i]);
			post_processing->readback.exposure_sync[i] = NULL;
		}
		if (post_processing->readback.histogram_sync[i]) {
			glDeleteSync(
			    post_processing->readback.histogram_sync[i]);
			post_processing->readback.histogram_sync[i] = NULL;
		}
	}
}

void pp_destroy_cached_shaders(PostProcess* post_processing)
{
	for (int i = 0; i < post_processing->shaders.shader_cache_count; i++) {
		if (post_processing->shaders.shader_cache[i].shader) {
			SHADER_SAFE_DESTROY(
			    post_processing->shaders.shader_cache[i].shader);
		}
	}
	post_processing->shaders.shader_cache_count = 0;
}

void postprocess_cleanup(PostProcess* post_processing)
{
	if (!post_processing) {
		return;
	}

	if (post_processing->gpu.dummy_uint_tex) {
		glDeleteTextures(1, &post_processing->gpu.dummy_uint_tex);
		post_processing->gpu.dummy_uint_tex = 0;
	}

	pp_destroy_readback_buffers(post_processing);
	pp_destroy_framebuffer(post_processing);
	pp_destroy_screen_quad(post_processing);

	GL_SAFE_DELETE_BUFFER(post_processing->gpu.settings_ubo);

	/* Main shader might be one of the cached ones.
	 * Nullify it if it's in the cache so it's not destroyed twice. */
	if (pp_is_shader_in_cache(
	        post_processing, post_processing->shaders.postprocess_shader)) {
		post_processing->shaders.postprocess_shader = NULL;
	}

	pp_destroy_cached_shaders(post_processing);

	/* Destroy postprocess_shader if it wasn't in the cache */
	SHADER_SAFE_DESTROY(post_processing->shaders.postprocess_shader);

	fx_bloom_cleanup(&post_processing->bloom_fx);
	fx_dof_cleanup(&post_processing->dof_fx);
	fx_auto_exposure_cleanup(&post_processing->auto_exposure_fx);
	fx_motion_blur_cleanup(&post_processing->motion_blur_fx);
	fx_lut_viz_cleanup(&post_processing->lut_viz_fx);
	fx_lut3d_cleanup(&post_processing->lut3d_fx);

	LOG_INFO("suckless-ogl.postprocess", "Post-processing cleaned up");
}
