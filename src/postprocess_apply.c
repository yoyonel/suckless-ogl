#include "effects/effect_context.h"
#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_lut3d.h"
#include "effects/fx_lut_viz.h"
#include "effects/fx_motion_blur.h"
#include "gl_debug.h"
#include "postprocess_internal.h"
#include "profiler.h"
#include "render_utils.h"

void postprocess_begin(PostProcess* post_processing)
{
	/* Rendre dans notre framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->gpu.scene_fbo);
	glClearStencil(0);
	glClear((GLbitfield)GL_COLOR_BUFFER_BIT |
	        (GLbitfield)GL_DEPTH_BUFFER_BIT |
	        (GLbitfield)GL_STENCIL_BUFFER_BIT);
}

void postprocess_end(PostProcess* post_processing)
{
	GPU_STAGE_PROFILER(post_processing->gpu_profiler, "Post-Process",
	                   GPU_PROFILER_POSTPROCESS_COLOR);

	/* Flush all MRT scene buffer writes (color, velocity, depth/stencil)
	 * so they are visible to subsequent compute shader texture fetches.
	 * Without this, compute shaders reading velocity_tex may see stale
	 * data when intermediate rasterization passes (bloom, DoF, AE)
	 * change the FBO binding between scene render and MB compute. */
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

	/* Bind shared screen quad VAO once for all fullscreen passes
	 * (Bloom, DoF, AE downsample, Final Composite). */
	glBindVertexArray(post_processing->gpu.screen_quad_vao);

	/* Générer le bloom (si activé) avant de binder le framebuffer par
	 * défaut */
	if (postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler, "Bloom",
		                   GPU_PROFILER_BLOOM_COLOR);
		gl_debug_push_group("PostFX_Bloom");
		const EffectContext bloom_ctx = {
		    .src_tex = post_processing->gpu.scene_color_tex,
		    .width = post_processing->width,
		    .height = post_processing->height,
		};
		fx_bloom_render(&post_processing->bloom_fx,
		                &post_processing->bloom, &bloom_ctx);
		gl_debug_pop_group();
	}

	/* DoF Blur Pass (if DoF enabled) */
	/* We reuse bloom_downsample to get a filtered 1/2 res version of the
	 * scene */
	if (postprocess_is_enabled(post_processing, POSTFX_DOF) ||
	    postprocess_is_enabled(post_processing, POSTFX_DOF_DEBUG)) {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler, "DoF",
		                   GPU_PROFILER_DOF_COLOR);
		gl_debug_push_group("PostFX_DepthOfField");
		{
			const EffectContext dof_ctx = {
			    .src_tex = post_processing->gpu.scene_color_tex,
			    .width = post_processing->width,
			    .height = post_processing->height,
			};
			fx_dof_render(&post_processing->dof_fx,
			              &post_processing->dof,
			              &post_processing->bloom_fx, &dof_ctx);
		}
		gl_debug_pop_group();
	}

	/* Auto Exposure Pass & Debug Histogram */
	bool ae_active =
	    postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE) != 0;
	bool debug_active =
	    postprocess_is_enabled(post_processing, POSTFX_EXPOSURE_DEBUG) != 0;

	if (ae_active || debug_active) {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler,
		                   "Auto Exposure",
		                   GPU_PROFILER_AUTO_EXPOSURE_COLOR);

		gl_debug_push_group("PostFX_AutoExposure");

		/* We always need the downsample/luminance pass if either AE or
		 * Debug is on */
		{
			const EffectContext ae_ctx = {
			    .src_tex = post_processing->gpu.scene_color_tex,
			    .width = post_processing->width,
			    .height = post_processing->height,
			    .delta_time = post_processing->delta_time,
			    .gpu_profiler = post_processing->gpu_profiler,
			};
			fx_auto_exposure_render(
			    &post_processing->auto_exposure_fx,
			    &post_processing->auto_exposure, &ae_ctx);
		}

		/* Trigger Async Readbacks for current frame (will be ready in
		 * next frames) */
		int write_idx =
		    (int)((post_processing->readback.frame_count + 1) % 2);

		if (ae_active &&
		    !post_processing->readback.exposure_sync[write_idx]) {
			glBindBuffer(
			    GL_PIXEL_PACK_BUFFER,
			    post_processing->readback.exposure_pbo[write_idx]);
			glBindTexture(
			    GL_TEXTURE_2D,
			    post_processing->auto_exposure_fx.exposure_tex);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, 0);
			post_processing->readback.exposure_sync[write_idx] =
			    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}

		if (debug_active &&
		    !post_processing->readback.histogram_sync[write_idx]) {
			glBindBuffer(
			    GL_PIXEL_PACK_BUFFER,
			    post_processing->readback.histogram_pbo[write_idx]);
			glBindTexture(
			    GL_TEXTURE_2D,
			    post_processing->auto_exposure_fx.downsample_tex);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, 0);
			post_processing->readback.histogram_sync[write_idx] =
			    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		gl_debug_pop_group(); /* PostFX_AutoExposure */
	}

	/* Motion Blur Pre-Pass (Compute) - Also needed for debug modes */
	if (postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR) ||
	    postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR_DEBUG) ||
	    postprocess_is_enabled(post_processing,
	                           POSTFX_VECTOR_FIELD_DEBUG)) {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler, "MB Compute",
		                   GPU_PROFILER_MOTION_BLUR_COLOR);
		gl_debug_push_group("PostFX_MotionBlur_Compute");
		{
			const EffectContext mb_ctx = {
			    .velocity_tex = post_processing->gpu.velocity_tex,
			    .width = post_processing->width,
			    .height = post_processing->height,
			};
			fx_motion_blur_render(&post_processing->motion_blur_fx,
			                      &mb_ctx);
		}
		gl_debug_pop_group();
	}

	/* === Final Composite: fullscreen quad with all fragment effects ===
	 * This draw call includes MB sampling, CA, DoF mix, Bloom mix,
	 * Exposure, Tonemapping, FXAA, Vignette, Grain, etc.
	 * The cost of Motion Blur fragment work is measured HERE. */
	{
		gl_debug_push_group("PostFX_Final_Composite");

		/* Retour au framebuffer par défaut */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, post_processing->width,
		           post_processing->height);
		glClear(GL_COLOR_BUFFER_BIT);

		/* Désactiver le depth test pour le quad */
		glDisable(GL_DEPTH_TEST);

		/* Utiliser le shader de post-processing */
		shader_use(post_processing->shaders.postprocess_shader);

		/* Bind la texture de la scène */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_SCENE);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->gpu.scene_color_tex);

		/* Bind la texture de Bloom */
		GLuint bloom_tex = 0;
		if (postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
			int step = post_processing->bloom_fx.debug_step;
			int mip_idx = post_processing->bloom_fx.debug_mip;

			if (step == 1 || step == 2 || step == 3) {
				/* Prefilter (step 1) uses mip 0.
				   Downsample (step 2) uses selected mip.
				   Upsample (step 3) uses selected mip. */
				bloom_tex =
				    post_processing->bloom_fx.mips[mip_idx]
				        .texture;
			} else {
				/* Final (step 0) or normal mode: use
				 * reconstructed mip 0
				 */
				bloom_tex =
				    post_processing->bloom_fx.mips[0].texture;
			}
		}

		render_utils_bind_texture_safe(
		    GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_BLOOM, bloom_tex,
		    post_processing->gpu.dummy_black_tex);

		/* Upload LUT3D params */
		if (postprocess_is_enabled(post_processing, POSTFX_LUT3D)) {
			fx_lut3d_upload_params(
			    post_processing->shaders.postprocess_shader,
			    &post_processing->lut3d);
		}

		/* Bind la texture de Profondeur (pour le DoF) */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_DEPTH);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->gpu.scene_depth_tex);

		/* Bind Exposure Texture (Unit 3) */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_EXPOSURE);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->auto_exposure_fx.exposure_tex);

		/* Bind Velocity Texture (Unit 4) - use safe bind to handle
		 * resize */
		render_utils_bind_texture_safe(
		    GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_VELOCITY,
		    post_processing->gpu.velocity_tex,
		    post_processing->gpu.dummy_black_tex);

		/* Bind Neighbor Max Texture (Unit 5) */
		glActiveTexture(GL_TEXTURE0 +
		                POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->motion_blur_fx.neighbor_max_tex);

		/* Bind DoF Blurred Texture (Unit 6) */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_DOF_BLUR);
		glBindTexture(GL_TEXTURE_2D, post_processing->dof_fx.blur_tex);

		/* Bind Stencil Texture View (Unit 7) */
		render_utils_bind_texture_safe(
		    GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_STENCIL,
		    post_processing->gpu.scene_stencil_view,
		    post_processing->gpu.dummy_uint_tex);

		/* Upload UBO: always update time/effects header, full rebuild
		 * only when parameters changed (ubo_dirty). */
		PROFILE_ZONE(ubo_upload_ctx, "PostProcess UBO Upload");
		glBindBuffer(GL_UNIFORM_BUFFER,
		             post_processing->gpu.settings_ubo);
		if (post_processing->ubo_dirty) {
			PostProcessUBO ubo = {0};
			ubo.active_effects = post_processing->active_effects;
			ubo.time = post_processing->time;
			ubo.screen_texel_size[0] =
			    1.0F / (float)post_processing->width;
			ubo.screen_texel_size[1] =
			    1.0F / (float)post_processing->height;

			ubo.vignette_intensity =
			    post_processing->vignette.intensity;
			ubo.vignette_smoothness =
			    post_processing->vignette.smoothness;
			ubo.vignette_roundness =
			    post_processing->vignette.roundness;

			ubo.grain_intensity = post_processing->grain.intensity;
			ubo.grain_intensity_shadows =
			    post_processing->grain.intensity_shadows;
			ubo.grain_intensity_midtones =
			    post_processing->grain.intensity_midtones;
			ubo.grain_intensity_highlights =
			    post_processing->grain.intensity_highlights;
			ubo.grain_shadows_max =
			    post_processing->grain.shadows_max;
			ubo.grain_highlights_min =
			    post_processing->grain.highlights_min;
			ubo.grain_texel_size =
			    post_processing->grain.texel_size;

			ubo.exposure_manual =
			    post_processing->exposure.exposure;
			ubo.chrom_abbr_strength =
			    post_processing->chrom_abbr.strength;

			ubo.wb_temperature =
			    post_processing->white_balance.temperature;
			ubo.wb_tint = post_processing->white_balance.tint;

			ubo.grading_saturation =
			    post_processing->color_grading.saturation;
			ubo.grading_contrast =
			    post_processing->color_grading.contrast;
			ubo.grading_gamma =
			    post_processing->color_grading.gamma;
			ubo.grading_gain = post_processing->color_grading.gain;
			ubo.grading_offset =
			    post_processing->color_grading.offset;
			ubo.grading_lift = post_processing->color_grading.lift;

			ubo.tonemap_slope = post_processing->tonemapper.slope;
			ubo.tonemap_toe = post_processing->tonemapper.toe;
			ubo.tonemap_shoulder =
			    post_processing->tonemapper.shoulder;
			ubo.tonemap_black_clip =
			    post_processing->tonemapper.black_clip;
			ubo.tonemap_white_clip =
			    post_processing->tonemapper.white_clip;

			ubo.bloom_intensity = post_processing->bloom.intensity;
			ubo.bloom_threshold = post_processing->bloom.threshold;
			ubo.bloom_soft_threshold =
			    post_processing->bloom.soft_threshold;
			ubo.bloom_radius = post_processing->bloom.radius;

			ubo.dof_focal_distance =
			    post_processing->dof.focal_distance;
			ubo.dof_focal_range = post_processing->dof.focal_range;
			ubo.dof_bokeh_scale = post_processing->dof.bokeh_scale;
			ubo.dof_anamorphic_ratio =
			    post_processing->dof.anamorphic_ratio;

			ubo.mb_intensity =
			    post_processing->motion_blur.intensity;
			ubo.mb_max_velocity =
			    post_processing->motion_blur.max_velocity;
			ubo.mb_samples = post_processing->motion_blur.samples;

			ubo.fxaa_quality_subpix = post_processing->fxaa.subpix;
			ubo.fxaa_quality_edge_threshold =
			    post_processing->fxaa.edge_threshold;
			ubo.fxaa_quality_edge_threshold_min =
			    post_processing->fxaa.edge_threshold_min;

			ubo.banding_mode = post_processing->banding.mode;
			ubo.banding_levels = post_processing->banding.levels;
			ubo.banding_dither_strength =
			    post_processing->banding.dither_strength;
			ubo.banding_perceptual_gamma =
			    post_processing->banding.perceptual_gamma;
			ubo.banding_channel_levels[0] =
			    post_processing->banding.channel_levels[0];
			ubo.banding_channel_levels[1] =
			    post_processing->banding.channel_levels[1];
			ubo.banding_channel_levels[2] =
			    post_processing->banding.channel_levels[2];

			ubo.fog_density = post_processing->fog.density;
			ubo.fog_start = post_processing->fog.start;
			ubo.fog_height_falloff =
			    post_processing->fog.height_falloff;
			ubo.fog_color[0] = post_processing->fog.color[0];
			ubo.fog_color[1] = post_processing->fog.color[1];
			ubo.fog_color[2] = post_processing->fog.color[2];

			ubo.lut3d_intensity = post_processing->lut3d.intensity;

			glBufferSubData(GL_UNIFORM_BUFFER, 0,
			                sizeof(PostProcessUBO), &ubo);
			post_processing->ubo_dirty = false;
		} else {
			/* Only update time (offset 4 bytes) and active_effects
			 * (offset 0) which change every frame */
			struct {
				uint32_t active_effects;
				float time;
			} header = {post_processing->active_effects,
			            post_processing->time};
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(header),
			                &header);
		}
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		PROFILE_ZONE_END(ubo_upload_ctx);

		/* Dessiner le quad */
		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

		/* Cleanup texture unit bindings to avoid leaking state into UI
		 * or next frame, which can trigger driver validation warnings.
		 */
		render_utils_reset_texture_units(
		    0, POSTPROCESS_TEX_UNIT_STENCIL + 1,
		    post_processing->gpu.dummy_black_tex);

		/* Render LUT Cube Visualization (if enabled) */
		{
			const EffectContext lut_viz_ctx = {
			    .width = post_processing->width,
			    .height = post_processing->height,
			};
			fx_lut_viz_render(&post_processing->lut_viz_fx,
			                  post_processing->lut3d.texture,
			                  &lut_viz_ctx);
		}

		gl_debug_pop_group(); /* PostFX_Final_Composite */
	}

	/* Unbind shared VAO after all fullscreen passes are complete */

	/* Réactiver le depth test */
	glEnable(GL_DEPTH_TEST);
}
