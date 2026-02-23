#include "postprocess.h"

#include "app_settings.h"
#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_motion_blur.h"
#include "gl_common.h"
#include "log.h"
#include "render_utils.h"
#include "shader.h"
#include "utils.h"
#include <cglm/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static int create_framebuffer(PostProcess* post_processing);
static void destroy_framebuffer(PostProcess* post_processing);
static void destroy_screen_quad(PostProcess* post_processing);
static bool is_shader_in_cache(PostProcess* post_processing, Shader* shader);

/* Texture Units */
enum {
	POSTPROCESS_TEX_UNIT_SCENE = 0,
	POSTPROCESS_TEX_UNIT_BLOOM = 1,
	POSTPROCESS_TEX_UNIT_DEPTH = 2,
	POSTPROCESS_TEX_UNIT_EXPOSURE = 3,
	POSTPROCESS_TEX_UNIT_VELOCITY = 4,
	POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX = 5,
	POSTPROCESS_TEX_UNIT_DOF_BLUR = 6,
	POSTPROCESS_TEX_UNIT_STENCIL = 7
};

/* Compute Shader Constants */
enum { POSTPROCESS_COMPUTE_GROUP_SIZE = 16 };

int postprocess_init(PostProcess* post_processing,
                     GPUProfiler* external_profiler, int width, int height)
{
	*post_processing = (PostProcess){0};

	post_processing->gpu_profiler = external_profiler;

	post_processing->width = width;
	post_processing->height = height;
	post_processing->time = 0.0F;
	post_processing->is_optimized = false;
	post_processing->ubo_dirty = true;
	post_processing->compiled_flags = ~0U;

	post_processing->shader_cache_count = 0;
	post_processing->banding_preset_idx = 0;

	/* Paramètres par défaut */
	post_processing->vignette.intensity = DEFAULT_VIGNETTE_INTENSITY;
	post_processing->vignette.smoothness = DEFAULT_VIGNETTE_SMOOTHNESS;
	post_processing->vignette.roundness = DEFAULT_VIGNETTE_ROUNDNESS;
	post_processing->grain.intensity = DEFAULT_GRAIN_INTENSITY;
	post_processing->grain.intensity_shadows = 1.0F;
	post_processing->grain.intensity_midtones = 1.0F;
	post_processing->grain.intensity_highlights = 1.0F;
	post_processing->grain.shadows_max = DEFAULT_GRAIN_SHADOWS_MAX;
	post_processing->grain.highlights_min = DEFAULT_GRAIN_HIGHLIGHTS_MIN;
	post_processing->grain.texel_size = DEFAULT_GRAIN_TEXEL_SIZE;

	post_processing->exposure.exposure = DEFAULT_EXPOSURE;
	post_processing->chrom_abbr.strength = DEFAULT_CHROM_ABBR_STRENGTH;

	/* Bloom defaults (Off) */
	post_processing->bloom.intensity = DEFAULT_BLOOM_INTENSITY;
	post_processing->bloom.threshold = DEFAULT_BLOOM_THRESHOLD;
	post_processing->bloom.soft_threshold = DEFAULT_BLOOM_SOFT_THRESHOLD;
	post_processing->bloom.radius = DEFAULT_BLOOM_RADIUS;

	/* White Balance Defaults */
	post_processing->white_balance.temperature = DEFAULT_WB_TEMP;
	post_processing->white_balance.tint = DEFAULT_WB_TINT;

	/* Tonemapper Defaults */
	post_processing->tonemapper.slope = DEFAULT_FILMIC_SLOPE;
	post_processing->tonemapper.toe = DEFAULT_FILMIC_TOE;
	post_processing->tonemapper.shoulder = DEFAULT_FILMIC_SHOULDER;
	post_processing->tonemapper.black_clip = DEFAULT_FILMIC_BLACK_CLIP;
	post_processing->tonemapper.white_clip = DEFAULT_FILMIC_WHITE_CLIP;

	/* Initialisation Color Grading (Neutre) */
	post_processing->color_grading.saturation = 1.0F;
	post_processing->color_grading.contrast = 1.0F;
	post_processing->color_grading.gamma = 1.0F;
	post_processing->color_grading.gain = 1.0F;
	post_processing->color_grading.offset = 0.0F;

	/* Initialisation Auto Exposure (Stabilisé) */
	post_processing->auto_exposure.min_luminance =
	    EXPOSURE_MIN_LUM; /* Limite le boost max à x2 (1.0 / 0.5) */
	post_processing->auto_exposure.max_luminance = EXPOSURE_DEFAULT_MAX_LUM;
	post_processing->auto_exposure.speed_up = EXPOSURE_SPEED_UP;
	post_processing->auto_exposure.speed_down = EXPOSURE_SPEED_DOWN;
	post_processing->auto_exposure.key_value = EXPOSURE_DEFAULT_KEY_VALUE;

	/* Initialisation Motion Blur */
	if (!fx_motion_blur_init(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create motion blur resources");
		/* On continue quand même */
	}

	/* Initialisation FXAA */
	post_processing->fxaa.subpix = DEFAULT_FXAA_SUBPIX;
	post_processing->fxaa.edge_threshold = DEFAULT_FXAA_EDGE_THRESHOLD;
	post_processing->fxaa.edge_threshold_min =
	    DEFAULT_FXAA_EDGE_THRESHOLD_MIN;

	/* Initialisation Banding */
	post_processing->banding.mode = BANDING_MODE_LINEAR;
	post_processing->banding.levels = DEFAULT_BANDING_LEVELS;
	post_processing->banding.dither_strength = 0.0F;
	post_processing->banding.perceptual_gamma = 1.0F;
	post_processing->banding.channel_levels[0] = DEFAULT_BANDING_LEVELS;
	post_processing->banding.channel_levels[1] = DEFAULT_BANDING_LEVELS;
	post_processing->banding.channel_levels[2] = DEFAULT_BANDING_LEVELS;

	/* Effets par défaut définis dans postprocess.h */
	post_processing->active_effects = DEFAULT_ACTIVE_EFFECTS;

	/* Créer le framebuffer */
	if (!create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create framebuffer");
		return 0;
	}

	/* Créer les ressources Bloom */
	if (!fx_bloom_init(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create bloom resources");
		destroy_framebuffer(post_processing);
		return 0;
	}

	/* Créer le quad plein écran */
	render_utils_create_fullscreen_quad(&post_processing->screen_quad_vao,
	                                    &post_processing->screen_quad_vbo);
	if (post_processing->screen_quad_vao == 0) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create screen quad");
		destroy_framebuffer(post_processing);
		fx_bloom_cleanup(post_processing);
		return 0;
	}

	/* Charger le shader de post-processing (Optimized Mode) */
	postprocess_compile_optimized(post_processing,
	                              post_processing->active_effects);

	/* Initialize UBO */
	glGenBuffers(1, &post_processing->settings_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, post_processing->settings_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(PostProcessUBO), NULL,
	             GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, post_processing->settings_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	if (!fx_auto_exposure_init(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create auto exposure resources");
		destroy_framebuffer(post_processing);
		fx_bloom_cleanup(post_processing);
		fx_dof_cleanup(post_processing);
		destroy_screen_quad(post_processing);
		return 0;
	}

	/* Créer les ressources DoF */
	if (!fx_dof_init(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create dof resources");
		destroy_framebuffer(post_processing);
		fx_bloom_cleanup(post_processing);
		destroy_screen_quad(post_processing);
		return 0;
	}

	LOG_INFO("suckless-ogl.postprocess",
	         "Post-processing initialized (%dx%d)", width, height);

	return 1;
}

void postprocess_set_dummy_textures(PostProcess* post_processing,
                                    GLuint dummy_black)
{
	post_processing->dummy_black_tex = dummy_black;
	LOG_INFO("suckless-ogl.postprocess", "Dummy texture set: %u",
	         dummy_black);
}

static void destroy_framebuffer(PostProcess* post_processing)
{
	GL_SAFE_DELETE_TEXTURE(post_processing->scene_color_tex);
	GL_SAFE_DELETE_TEXTURE(post_processing->velocity_tex);
	GL_SAFE_DELETE_TEXTURE(post_processing->scene_depth_tex);
	GL_SAFE_DELETE_TEXTURE(post_processing->scene_stencil_view);
	GL_SAFE_DELETE_FRAMEBUFFER(post_processing->scene_fbo);

	/* Bridge Unit 0 with dummy to avoid invalid state warnings during
	 * resize
	 */
	render_utils_bind_texture_safe(GL_TEXTURE0, 0,
	                               post_processing->dummy_black_tex);
}

static void destroy_screen_quad(PostProcess* post_processing)
{
	GL_SAFE_DELETE_VAO(post_processing->screen_quad_vao);
	GL_SAFE_DELETE_BUFFER(post_processing->screen_quad_vbo);
}

void postprocess_cleanup(PostProcess* post_processing)
{
	if (!post_processing) {
		return;
	}

	destroy_framebuffer(post_processing);
	destroy_screen_quad(post_processing);

	GL_SAFE_DELETE_BUFFER(post_processing->settings_ubo);

	/* Main shader might be one of the cached ones.
	 * Nullify it if it's in the cache so it's not destroyed twice. */
	if (is_shader_in_cache(post_processing,
	                       post_processing->postprocess_shader)) {
		post_processing->postprocess_shader = NULL;
	}

	/* Destroy cached shaders */
	for (int i = 0; i < post_processing->shader_cache_count; i++) {
		if (post_processing->shader_cache[i].shader) {
			/* SHADER_SAFE_DESTROY will handle internal program +
			 * struct free */
			SHADER_SAFE_DESTROY(
			    post_processing->shader_cache[i].shader);
		}
	}
	post_processing->shader_cache_count = 0;

	/* Destroy postprocess_shader if it wasn't in the cache */
	SHADER_SAFE_DESTROY(post_processing->postprocess_shader);

	fx_bloom_cleanup(post_processing);
	fx_dof_cleanup(post_processing);
	fx_auto_exposure_cleanup(post_processing);
	fx_motion_blur_cleanup(post_processing);

	LOG_INFO("suckless-ogl.postprocess", "Post-processing cleaned up");
}

void postprocess_resize(PostProcess* post_processing, int width, int height)
{
	if (post_processing->width == width &&
	    post_processing->height == height) {
		return;
	}

	post_processing->width = width;
	post_processing->height = height;
	post_processing->ubo_dirty = true;

	/* Recréer le framebuffer avec les nouvelles dimensions */
	destroy_framebuffer(post_processing);
	if (!create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize framebuffer");
	}

	fx_bloom_cleanup(post_processing);
	if (!fx_bloom_init(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize bloom resources");
	}

	fx_dof_resize(post_processing);
	fx_motion_blur_resize(post_processing);

	/* Final Bridge: Ensure ALL used units are in a valid state.
	 * NVIDIA driver validates units used by the last shader before resize.
	 */
	render_utils_reset_texture_units(GL_TEXTURE0,
	                                 POSTPROCESS_TEX_UNIT_DOF_BLUR + 1,
	                                 post_processing->dummy_black_tex);

	/* Bind the real velocity texture on unit 4 to prevent sampling errors
	 */
	if (post_processing->velocity_tex) {
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_VELOCITY);
		glBindTexture(GL_TEXTURE_2D, post_processing->velocity_tex);
	}

	/* Reset to Unit 0 for subsequent generic bindings */
	glActiveTexture(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	LOG_INFO("suckless-ogl.postprocess", "Resized to %dx%d", width, height);
}

static void postprocess_on_state_change(PostProcess* post_processing)
{
	if (post_processing->is_optimized) {
		if (post_processing->active_effects ==
		    post_processing->compiled_flags) {
			return;
		}
		LOG_INFO(
		    "suckless-ogl.postprocess",
		    "State changed in optimized mode - recompiling shader...");
		postprocess_compile_optimized(post_processing,
		                              post_processing->active_effects);
	}
}

void postprocess_enable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects |= (unsigned int)effect;
	postprocess_on_state_change(post_processing);
}

void postprocess_disable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects &= ~(unsigned int)effect;
	postprocess_on_state_change(post_processing);
}

void postprocess_toggle(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects ^= (unsigned int)effect;
	postprocess_on_state_change(post_processing);
}

int postprocess_is_enabled(PostProcess* post_processing,
                           PostProcessEffect effect)
{
	return (post_processing->active_effects & (unsigned int)effect) != 0;
}

void postprocess_set_vignette(PostProcess* post_processing, float intensity,
                              float smoothness, float roundness)
{
	post_processing->vignette.intensity = intensity;
	post_processing->vignette.smoothness = smoothness;
	post_processing->vignette.roundness = roundness;
	post_processing->ubo_dirty = true;
}

void postprocess_set_grain(PostProcess* post_processing, float intensity)
{
	post_processing->grain.intensity = intensity;
	post_processing->ubo_dirty = true;
}

void postprocess_set_exposure(PostProcess* post_processing, float exposure)
{
	post_processing->exposure.exposure = exposure;
	post_processing->ubo_dirty = true;
}

void postprocess_set_chrom_abbr(PostProcess* post_processing, float strength)
{
	post_processing->chrom_abbr.strength = strength;
	post_processing->ubo_dirty = true;
}

void postprocess_set_white_balance(PostProcess* post_processing,
                                   float temperature, float tint)
{
	post_processing->white_balance.temperature = temperature;
	post_processing->white_balance.tint = tint;
	post_processing->ubo_dirty = true;
}

void postprocess_set_color_grading(PostProcess* post_processing,
                                   float saturation, float contrast,
                                   float gamma, float gain, float offset)
{
	post_processing->color_grading.saturation = saturation;
	post_processing->color_grading.contrast = contrast;
	post_processing->color_grading.gamma = gamma;
	post_processing->color_grading.gain = gain;
	post_processing->color_grading.offset = offset;
	post_processing->ubo_dirty = true;
}

void postprocess_set_tonemapper(PostProcess* post_processing, float slope,
                                float toe, float shoulder, float black_clip,
                                float white_clip)
{
	post_processing->tonemapper.slope = slope;
	post_processing->tonemapper.toe = toe;
	post_processing->tonemapper.shoulder = shoulder;
	post_processing->tonemapper.black_clip = black_clip;
	post_processing->tonemapper.white_clip = white_clip;
	post_processing->ubo_dirty = true;
}

void postprocess_set_bloom(PostProcess* post_processing, float intensity,
                           float threshold, float soft_threshold)
{
	post_processing->bloom.intensity = intensity;
	post_processing->bloom.threshold = threshold;
	post_processing->bloom.soft_threshold = soft_threshold;
	post_processing->ubo_dirty = true;
}

void postprocess_set_dof(PostProcess* post_processing, float focal_distance,
                         float focal_range, float bokeh_scale)
{
	post_processing->dof.focal_distance = focal_distance;
	post_processing->dof.focal_range = focal_range;
	post_processing->dof.bokeh_scale = bokeh_scale;
	post_processing->ubo_dirty = true;
}

float postprocess_get_exposure(PostProcess* post_processing)
{
	return fx_auto_exposure_get_current_exposure(post_processing);
}

void postprocess_set_auto_exposure(PostProcess* post_processing,
                                   float min_luminance, float max_luminance,
                                   float speed_up, float speed_down,
                                   float key_value)
{
	post_processing->auto_exposure.min_luminance = min_luminance;
	post_processing->auto_exposure.max_luminance = max_luminance;
	post_processing->auto_exposure.speed_up = speed_up;
	post_processing->auto_exposure.speed_down = speed_down;
	post_processing->auto_exposure.key_value = key_value;
	post_processing->ubo_dirty = true;
}

void postprocess_set_fxaa(PostProcess* post_processing, float subpix,
                          float edge_threshold, float edge_threshold_min)
{
	post_processing->fxaa.subpix = subpix;
	post_processing->fxaa.edge_threshold = edge_threshold;
	post_processing->fxaa.edge_threshold_min = edge_threshold_min;
	post_processing->ubo_dirty = true;
}

void postprocess_set_banding(PostProcess* post_processing, BandingMode mode,
                             float levels)
{
	post_processing->banding.mode = (int32_t)mode;
	post_processing->banding.levels = levels;
	post_processing->ubo_dirty = true;
}

void postprocess_set_banding_dither(PostProcess* post_processing,
                                    float strength)
{
	post_processing->banding.dither_strength = strength;
	post_processing->ubo_dirty = true;
}

void postprocess_set_banding_perceptual(PostProcess* post_processing,
                                        float gamma)
{
	post_processing->banding.perceptual_gamma = gamma;
	post_processing->ubo_dirty = true;
}

void postprocess_set_banding_channels(PostProcess* post_processing, float red,
                                      float green, float blue)
{
	post_processing->banding.channel_levels[0] = red;
	post_processing->banding.channel_levels[1] = green;
	post_processing->banding.channel_levels[2] = blue;
	post_processing->ubo_dirty = true;
}

void postprocess_set_grading_ue_default(PostProcess* post_processing)
{
	/* * Valeurs par défaut d'Unreal Engine (Section "Global").
	 * Le "look" UE vient de l'application de ces valeurs neutres
	 * combinées à la courbe de tone mapping ACES dans le shader.
	 */
	post_processing->color_grading.saturation =
	    1.0F;                                       /* Pas de changement */
	post_processing->color_grading.contrast = 1.0F; /* Pas de changement */
	post_processing->color_grading.gamma = 1.0F;    /* Pas de changement */
	post_processing->color_grading.gain = 1.0F;     /* Pas de changement */
	post_processing->color_grading.offset = 0.0F;   /* Pas de changement */
	post_processing->ubo_dirty = true;

	/* On s'assure que l'effet est activé pour passer dans le pipeline ACES
	 */
	postprocess_enable(post_processing, POSTFX_COLOR_GRADING);
}

void postprocess_apply_preset(PostProcess* post_processing,
                              const PostProcessPreset* preset)
{
	post_processing->active_effects = preset->active_effects;
	post_processing->vignette = preset->vignette;
	post_processing->grain = preset->grain;
	post_processing->exposure = preset->exposure;
	post_processing->chrom_abbr = preset->chrom_abbr;
	post_processing->white_balance = preset->white_balance;
	post_processing->color_grading = preset->color_grading;
	post_processing->tonemapper = preset->tonemapper;
	post_processing->bloom = preset->bloom;
	post_processing->dof = preset->dof;
	post_processing->fxaa = preset->fxaa;
	post_processing->banding = preset->banding;
	post_processing->ubo_dirty = true;

	postprocess_on_state_change(post_processing);
}

void postprocess_begin(PostProcess* post_processing)
{
	/* Rendre dans notre framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->scene_fbo);
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
	glBindVertexArray(post_processing->screen_quad_vao);

	/* Générer le bloom (si activé) avant de binder le framebuffer par
	 * défaut */
	if (postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler, "Bloom",
		                   GPU_PROFILER_BLOOM_COLOR);
		fx_bloom_render(post_processing);
	}

	/* DoF Blur Pass (if DoF enabled) */
	/* We reuse bloom_downsample to get a filtered 1/2 res version of the
	 * scene */
	if (postprocess_is_enabled(post_processing, POSTFX_DOF) ||
	    postprocess_is_enabled(post_processing, POSTFX_DOF_DEBUG)) {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler, "DoF",
		                   GPU_PROFILER_DOF_COLOR);
		fx_dof_render(post_processing);
	}

	/* Auto Exposure Pass */
	if (postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		// TODO: use RAII here !
		GPU_STAGE_PROFILER(post_processing->gpu_profiler,
		                   "Auto Exposure",
		                   GPU_PROFILER_AUTO_EXPOSURE_COLOR);
		fx_auto_exposure_render(post_processing);
	}

	/* Motion Blur Pre-Pass (Compute) - Also needed for debug modes */
	if (postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR) ||
	    postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR_DEBUG) ||
	    postprocess_is_enabled(post_processing,
	                           POSTFX_VECTOR_FIELD_DEBUG)) {
		GPU_STAGE_PROFILER(post_processing->gpu_profiler, "MB Compute",
		                   GPU_PROFILER_MOTION_BLUR_COLOR);
		fx_motion_blur_render(post_processing);
	}

	/* === Final Composite: fullscreen quad with all fragment effects ===
	 * This draw call includes MB sampling, CA, DoF mix, Bloom mix,
	 * Exposure, Tonemapping, FXAA, Vignette, Grain, etc.
	 * The cost of Motion Blur fragment work is measured HERE. */
	{
		GPU_STAGE_PROFILER(post_processing->gpu_profiler,
		                   "Final Composite",
		                   GPU_PROFILER_COMPOSITE_COLOR);

		/* Retour au framebuffer par défaut */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, post_processing->width,
		           post_processing->height);
		glClear(GL_COLOR_BUFFER_BIT);

		/* Désactiver le depth test pour le quad */
		glDisable(GL_DEPTH_TEST);

		/* Utiliser le shader de post-processing */
		shader_use(post_processing->postprocess_shader);

		/* Bind la texture de la scène */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_SCENE);
		glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);

		/* Bind la texture de Bloom */
		render_utils_bind_texture_safe(
		    GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_BLOOM,
		    postprocess_is_enabled(post_processing, POSTFX_BLOOM)
		        ? post_processing->bloom_fx.mips[0].texture
		        : 0,
		    post_processing->dummy_black_tex);

		/* Bind la texture de Profondeur (pour le DoF) */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_DEPTH);
		glBindTexture(GL_TEXTURE_2D, post_processing->scene_depth_tex);

		/* Bind Exposure Texture (Unit 3) */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_EXPOSURE);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->auto_exposure_fx.exposure_tex);

		/* Bind Velocity Texture (Unit 4) - use safe bind to handle
		 * resize */
		render_utils_bind_texture_safe(
		    GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_VELOCITY,
		    post_processing->velocity_tex,
		    post_processing->dummy_black_tex);

		/* Bind Neighbor Max Texture (Unit 5) */
		glActiveTexture(GL_TEXTURE0 +
		                POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->motion_blur_fx.neighbor_max_tex);

		/* Bind DoF Blurred Texture (Unit 6) */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_DOF_BLUR);
		glBindTexture(GL_TEXTURE_2D, post_processing->dof_fx.blur_tex);

		/* Bind Stencil Texture View (Unit 7) */
		glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_STENCIL);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->scene_stencil_view);

		/* Upload UBO: always update time/effects header, full rebuild
		 * only when parameters changed (ubo_dirty). */
		glBindBuffer(GL_UNIFORM_BUFFER, post_processing->settings_ubo);
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

		/* Dessiner le quad */
		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	}

	/* Unbind shared VAO after all fullscreen passes are complete */
	glBindVertexArray(0);

	/* Réactiver le depth test */
	glEnable(GL_DEPTH_TEST);
}

void postprocess_update_time(PostProcess* post_processing, float delta_time)
{
	post_processing->time += delta_time;
	post_processing->delta_time =
	    delta_time; /* Save dt for compute shader */
}

void postprocess_update_matrices(PostProcess* post_processing, mat4 view_proj)
{
	fx_motion_blur_update_matrices(post_processing, view_proj);
}

/* Fonctions privées */

static bool is_shader_in_cache(PostProcess* post_processing, Shader* shader)
{
	if (!shader) {
		return false;
	}
	for (int i = 0; i < post_processing->shader_cache_count; i++) {
		if (post_processing->shader_cache[i].shader == shader) {
			return true;
		}
	}
	return false;
}

/**
 * @brief Sets sampler uniform → texture unit bindings on the postprocess
 * shader. These are program state (not context state) and only need to be
 * set once per shader program, not every frame.
 */
static void setup_sampler_uniforms(PostProcess* post_processing)
{
	Shader* shader = post_processing->postprocess_shader;
	if (!shader) {
		return;
	}

	shader_use(shader);
	shader_set_int(shader, "screenTexture", POSTPROCESS_TEX_UNIT_SCENE);
	shader_set_int(shader, "bloomTexture", POSTPROCESS_TEX_UNIT_BLOOM);
	shader_set_int(shader, "depthTexture", POSTPROCESS_TEX_UNIT_DEPTH);
	shader_set_int(shader, "autoExposureTexture",
	               POSTPROCESS_TEX_UNIT_EXPOSURE);
	shader_set_int(shader, "velocityTexture",
	               POSTPROCESS_TEX_UNIT_VELOCITY);
	shader_set_int(shader, "neighborMaxTexture",
	               POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX);
	shader_set_int(shader, "dofBlurTexture", POSTPROCESS_TEX_UNIT_DOF_BLUR);
	shader_set_int(shader, "stencilTexture", POSTPROCESS_TEX_UNIT_STENCIL);
}

static int create_framebuffer(PostProcess* post_processing)
{
	/* Ensure Unit 0 is active for initial texture setup */
	glActiveTexture(GL_TEXTURE0);

	/* Créer le framebuffer */
	glGenFramebuffers(1, &post_processing->scene_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->scene_fbo);

	/* Créer la texture de couleur (HDR) */
	post_processing->scene_color_tex = render_utils_create_texture_2d(
	    post_processing->width, post_processing->height, GL_RGBA16F, 1,
	    "Scene Color (HDR)");
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	// render_utils sets LINEAR/LINEAR by default for levels=1
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, post_processing->scene_color_tex,
	                       0);

	/* Créer la texture de vélocité (GL_RG16F) */
	post_processing->velocity_tex = render_utils_create_texture_2d(
	    post_processing->width, post_processing->height, GL_RG16F, 1,
	    "Velocity Buffer");
	glBindTexture(GL_TEXTURE_2D, post_processing->velocity_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
	                       GL_TEXTURE_2D, post_processing->velocity_tex, 0);

	/* Neighbor Max Texture (RG16F) */

	/* Configurer les buffers de rendu (MRT) */
	GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
	glDrawBuffers(2, drawBuffers);

	/* Créer la texture de profondeur (D32F_S8 pour précision max + stencil)
	 */
	post_processing->scene_depth_tex = render_utils_create_texture_2d(
	    post_processing->width, post_processing->height,
	    GL_DEPTH32F_STENCIL8, 1, "Scene Depth (D32F_S8)");
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_depth_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
	                       GL_TEXTURE_2D, post_processing->scene_depth_tex,
	                       0);

	/* Créer une vue Texture View pour accéder au Stencil uniquement */
	glGenTextures(1, &post_processing->scene_stencil_view);
	/* Utiliser le même format compatible (class 64-bit/32F_S8) mais changer
	 * le mode de lecture */
	glTextureView(post_processing->scene_stencil_view, GL_TEXTURE_2D,
	              post_processing->scene_depth_tex, GL_DEPTH32F_STENCIL8, 0,
	              1, 0, 1);
	glObjectLabel(GL_TEXTURE, post_processing->scene_stencil_view, -1,
	              "Scene Stencil View");
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_stencil_view);
	/* Mode Stencil Index: Permet de lire le canal Stencil (uint) via
	 * usampler2D */
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE,
	                GL_STENCIL_INDEX);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	return render_utils_check_framebuffer("PostProcess Scene FBO");
}

enum { MAX_SHADER_DEFINES = 32, MAX_DEFINE_LENGTH = 64 };

typedef struct {
	PostProcessEffect flag;
	const char* name;
	const char* define_name;
} EffectMetadata;

static const EffectMetadata ALL_EFFECTS[] = {
    {POSTFX_VIGNETTE, "Vignette", "OPT_ENABLE_VIGNETTE"},
    {POSTFX_GRAIN, "Film Grain", "OPT_ENABLE_GRAIN"},
    {POSTFX_EXPOSURE, "Manual Exposure", "OPT_ENABLE_EXPOSURE"},
    {POSTFX_CHROM_ABBR, "Chromatic Aberration", "OPT_ENABLE_CHROM_ABBR"},
    {POSTFX_BLOOM, "Bloom", "OPT_ENABLE_BLOOM"},
    {POSTFX_COLOR_GRADING, "Color Grading", "OPT_ENABLE_COLOR_GRADING"},
    {POSTFX_DOF, "Depth of Field", "OPT_ENABLE_DOF"},
    {POSTFX_DOF_DEBUG, "DoF Debug View", "OPT_ENABLE_DOF_DEBUG"},
    {POSTFX_AUTO_EXPOSURE, "Auto-Exposure", "OPT_ENABLE_AUTO_EXPOSURE"},
    {POSTFX_EXPOSURE_DEBUG, "Exposure Debug View", "OPT_ENABLE_EXPOSURE_DEBUG"},
    {POSTFX_MOTION_BLUR, "Motion Blur", "OPT_ENABLE_MOTION_BLUR"},
    {POSTFX_MOTION_BLUR_DEBUG, "Motion Blur Debug View",
     "OPT_ENABLE_MOTION_BLUR_DEBUG"},
    {POSTFX_FXAA, "FXAA", "OPT_ENABLE_FXAA"},
    {POSTFX_FXAA_DEBUG, "FXAA Debug View", "OPT_ENABLE_FXAA_DEBUG"},
    {POSTFX_BANDING, "Banding", "OPT_ENABLE_BANDING"},
    {POSTFX_VECTOR_FIELD_DEBUG, "Vector Field Debug",
     "OPT_ENABLE_VECTOR_FIELD_DEBUG"},
    {POSTFX_STENCIL_DEBUG, "Stencil Debug View", "OPT_ENABLE_STENCIL_DEBUG"},
};

#define EFFECT_COUNT (sizeof(ALL_EFFECTS) / sizeof(ALL_EFFECTS[0]))

static void log_optimized_effects(unsigned int flags)
{
	LOG_INFO("suckless-ogl.postprocess",
	         "Compiled OPTIMIZED shader with effects:");

	for (size_t i = 0; i < EFFECT_COUNT; i++) {
		if ((flags & (unsigned int)ALL_EFFECTS[i].flag) != 0) {
			LOG_INFO("suckless-ogl.postprocess", "  ✓ %s",
			         ALL_EFFECTS[i].name);
		}
	}

	LOG_INFO("suckless-ogl.postprocess",
	         "Shader optimization complete (Flags: 0x%08X)", flags);
}

static Shader* find_shader_in_cache(PostProcess* post_processing,
                                    unsigned int static_flags)
{
	for (int i = 0; i < post_processing->shader_cache_count; i++) {
		if (post_processing->shader_cache[i].flags == static_flags) {
			/* Found it! Move to front (LRU policy) */
			if (i > 0) {
				ShaderCacheEntry entry =
				    post_processing->shader_cache[i];

				/* Manual shift to avoid insecure memmove
				 * warnings */
				for (int j = i; j > 0; j--) {
					post_processing->shader_cache[j] =
					    post_processing
					        ->shader_cache[j - 1];
				}

				post_processing->shader_cache[0] = entry;
			}
			return post_processing->shader_cache[0].shader;
		}
	}
	return NULL;
}

static void update_current_shader(PostProcess* post_processing,
                                  Shader* new_shader, bool is_optimized)
{
	/* Destroy previous shader only if it was dynamic (not in cache) */
	if (post_processing->postprocess_shader &&
	    !is_shader_in_cache(post_processing,
	                        post_processing->postprocess_shader)) {
		shader_destroy(post_processing->postprocess_shader);
	}

	post_processing->postprocess_shader = new_shader;
	post_processing->is_optimized = is_optimized;

	/* Sampler→unit bindings are per-program state.
	 * Set them once when the active shader changes. */
	setup_sampler_uniforms(post_processing);
}

void postprocess_compile_optimized(PostProcess* post_processing,
                                   unsigned int static_flags)
{
	/* Check cache first */
	Shader* cached = find_shader_in_cache(post_processing, static_flags);
	if (cached) {
		if (post_processing->postprocess_shader != cached) {
			update_current_shader(post_processing, cached, true);
			LOG_INFO("suckless-ogl.postprocess",
			         "Using CACHED shader for flags 0x%08X",
			         static_flags);
		}
		post_processing->compiled_flags = static_flags;
		return;
	}

	const char* defines[MAX_SHADER_DEFINES];
	int count = 0;
	char buffer[MAX_SHADER_DEFINES][MAX_DEFINE_LENGTH];

	for (size_t i = 0; i < EFFECT_COUNT; i++) {
		if (!safe_snprintf(
		        buffer[count], sizeof(buffer[count]), "%s %d",
		        ALL_EFFECTS[i].define_name,
		        ((static_flags & (unsigned int)ALL_EFFECTS[i].flag) !=
		         0))) {
			LOG_ERROR("suckless-ogl.postprocess",
			          "Failed to format shader define");
			return;
		}
		defines[count] = buffer[count];
		count++;
	}

	Shader* new_shader = shader_load_with_defines(
	    "shaders/postprocess.vert", "shaders/postprocess.frag", defines,
	    count);

	if (new_shader) {
		new_shader->silent_warnings = true;
		update_current_shader(post_processing, new_shader, true);
		post_processing->compiled_flags = static_flags;

		/* Add to cache */
		/* Move existing entries down to make room at index 0 */
		int move_count = post_processing->shader_cache_count;
		if (move_count == SHADER_CACHE_SIZE) {
			/* Cache full: Evict LRU (last entry) */
			shader_destroy(
			    post_processing->shader_cache[SHADER_CACHE_SIZE - 1]
			        .shader);
			move_count--; /* Only move first 31 items */
		} else {
			post_processing->shader_cache_count++;
		}

		if (move_count > 0) {
			/* Manual shift to avoid insecure memmove warnings */
			for (int j = move_count; j > 0; j--) {
				post_processing->shader_cache[j] =
				    post_processing->shader_cache[j - 1];
			}
		}

		post_processing->shader_cache[0].flags = static_flags;
		post_processing->shader_cache[0].shader = new_shader;

		log_optimized_effects(static_flags);
	} else {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to compile optimized shader");
	}
}

void postprocess_use_dynamic(PostProcess* post_processing)
{
	Shader* new_shader =
	    shader_load("shaders/postprocess.vert", "shaders/postprocess.frag");

	if (new_shader) {
		update_current_shader(post_processing, new_shader, false);
		LOG_INFO("suckless-ogl.postprocess",
		         "Switched to DYNAMIC shader");
	} else {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to compile dynamic shader");
	}
}
