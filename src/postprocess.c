#include "postprocess.h"

#include "gl_common.h"
#include "log.h"
#include "shader.h"
#include <string.h>

static int create_framebuffer(PostProcess* post_processing);
static int create_bloom_resources(PostProcess* post_processing);
static int create_screen_quad(PostProcess* post_processing);
static void destroy_framebuffer(PostProcess* post_processing);
static void destroy_bloom_resources(PostProcess* post_processing);
static void destroy_screen_quad(PostProcess* post_processing);
static void render_bloom(PostProcess* post_processing);
static void render_auto_exposure(PostProcess* post_processing);

/* TODO: move to gl_common.h and mutualize with skybox rendering */
enum { SCREEN_QUAD_VERTEX_COUNT = 6 };

/* Auto Exposure Constants */
static const float EXPOSURE_MIN_LUM = 0.5F;
static const float EXPOSURE_DEFAULT_MAX_LUM = 5000.0F;
static const float EXPOSURE_SPEED_UP = 2.0F;
static const float EXPOSURE_SPEED_DOWN = 1.0F;
static const float EXPOSURE_DEFAULT_KEY_VALUE = 1.0F;
static const float EXPOSURE_INITIAL_VAL = 0.5F;

/* Vertices pour un quad plein écran */
static const float screen_quad_vertices[SCREEN_QUAD_VERTEX_COUNT * (2 + 2)] =
    {/* positions     texCoords */
     -1.0F, 1.0F, 0.0F, 1.0F,  -1.0F, -1.0F,
     0.0F,  0.0F, 1.0F, -1.0F, 1.0F,  0.0F,

     -1.0F, 1.0F, 0.0F, 1.0F,  1.0F,  -1.0F,
     1.0F,  0.0F, 1.0F, 1.0F,  1.0F,  1.0F};

int postprocess_init(PostProcess* post_processing, int width, int height)
{
	*post_processing = (PostProcess){0};

	post_processing->width = width;
	post_processing->height = height;
	post_processing->time = 0.0F;

	/* Paramètres par défaut */
	post_processing->vignette.intensity = DEFAULT_VIGNETTE_INTENSITY;
	post_processing->vignette.extent = DEFAULT_VIGNETTE_EXTENT;
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

	post_processing->color_grading.offset = 0.0F;

	/* Initialisation Auto Exposure (Stabilisé) */
	/* Initialisation Auto Exposure (Stabilisé) */
	post_processing->auto_exposure.min_luminance =
	    EXPOSURE_MIN_LUM; /* Limite le boost max à x2 (1.0 / 0.5) */
	post_processing->auto_exposure.max_luminance = EXPOSURE_DEFAULT_MAX_LUM;
	post_processing->auto_exposure.speed_up = EXPOSURE_SPEED_UP;
	post_processing->auto_exposure.speed_down = EXPOSURE_SPEED_DOWN;
	post_processing->auto_exposure.key_value = EXPOSURE_DEFAULT_KEY_VALUE;

	/* Motion Blur Defaults */
	glm_mat4_identity(post_processing->previous_view_proj);

	/* Effets désactivés par défaut */
	post_processing->active_effects = 0;

	/* Créer le framebuffer */
	if (!create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create framebuffer");
		return 0;
	}

	/* Créer les ressources Bloom */
	if (!create_bloom_resources(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create bloom resources");
		destroy_framebuffer(post_processing);
		return 0;
	}

	/* Créer le quad plein écran */
	if (!create_screen_quad(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create screen quad");
		destroy_framebuffer(post_processing);
		destroy_bloom_resources(post_processing);
		return 0;
	}

	/* Charger le shader de post-processing */
	post_processing->postprocess_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/postprocess.frag");

	if (!post_processing->postprocess_shader) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to load postprocess shader");
		destroy_framebuffer(post_processing);
		destroy_bloom_resources(post_processing);
		destroy_screen_quad(post_processing);
		return 0;
	}

	post_processing->bloom_prefilter_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/bloom_prefilter.frag");
	post_processing->bloom_downsample_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/bloom_downsample.frag");
	post_processing->bloom_upsample_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/bloom_upsample.frag");

	post_processing->lum_downsample_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/lum_downsample.frag");

	post_processing->lum_adapt_shader =
	    shader_load_compute("shaders/lum_adapt.comp");

	if (!post_processing->bloom_prefilter_shader ||
	    !post_processing->bloom_downsample_shader ||
	    !post_processing->bloom_upsample_shader ||
	    !post_processing->lum_downsample_shader ||
	    !post_processing->lum_adapt_shader) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to load post-process shaders");
		/* On continue quand même */
	}

	LOG_INFO("suckless-ogl.postprocess",
	         "Post-processing initialized (%dx%d)", width, height);

	return 1;
}

void postprocess_cleanup(PostProcess* post_processing)
{
	destroy_framebuffer(post_processing);
	destroy_screen_quad(post_processing);

	if (post_processing->postprocess_shader) {
		glDeleteProgram(post_processing->postprocess_shader);
		post_processing->postprocess_shader = 0;
	}
	if (post_processing->bloom_prefilter_shader) {
		glDeleteProgram(post_processing->bloom_prefilter_shader);
		post_processing->bloom_prefilter_shader = 0;
	}
	if (post_processing->bloom_downsample_shader) {
		glDeleteProgram(post_processing->bloom_downsample_shader);
		post_processing->bloom_downsample_shader = 0;
	}
	if (post_processing->bloom_upsample_shader) {
		glDeleteProgram(post_processing->bloom_upsample_shader);
		post_processing->bloom_upsample_shader = 0;
	}
	destroy_bloom_resources(post_processing);

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

	/* Recréer le framebuffer avec les nouvelles dimensions */
	destroy_framebuffer(post_processing);
	if (!create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize framebuffer");
	}

	destroy_bloom_resources(post_processing);
	if (!create_bloom_resources(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize bloom resources");
	}

	LOG_INFO("suckless-ogl.postprocess", "Resized to %dx%d", width, height);
}

void postprocess_enable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects |= (unsigned int)effect;
}

void postprocess_disable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects &= ~(unsigned int)effect;
}

void postprocess_toggle(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects ^= (unsigned int)effect;
}

int postprocess_is_enabled(PostProcess* post_processing,
                           PostProcessEffect effect)
{
	return (post_processing->active_effects & (unsigned int)effect) != 0;
}

void postprocess_set_vignette(PostProcess* post_processing, float intensity,
                              float extent)
{
	post_processing->vignette.intensity = intensity;
	post_processing->vignette.extent = extent;
}

void postprocess_set_grain(PostProcess* post_processing, float intensity)
{
	post_processing->grain.intensity = intensity;
}

void postprocess_set_exposure(PostProcess* post_processing, float exposure)
{
	post_processing->exposure.exposure = exposure;
}

void postprocess_set_chrom_abbr(PostProcess* post_processing, float strength)
{
	post_processing->chrom_abbr.strength = strength;
}

void postprocess_set_white_balance(PostProcess* post_processing,
                                   float temperature, float tint)
{
	post_processing->white_balance.temperature = temperature;
	post_processing->white_balance.tint = tint;
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
}

void postprocess_set_bloom(PostProcess* post_processing, float intensity,
                           float threshold, float soft_threshold)
{
	post_processing->bloom.intensity = intensity;
	post_processing->bloom.threshold = threshold;
	post_processing->bloom.soft_threshold = soft_threshold;
}

void postprocess_set_dof(PostProcess* post_processing, float focal_distance,
                         float focal_range, float bokeh_scale)
{
	post_processing->dof.focal_distance = focal_distance;
	post_processing->dof.focal_range = focal_range;
	post_processing->dof.bokeh_scale = bokeh_scale;
}

float postprocess_get_exposure(PostProcess* post_processing)
{
	float pixel[4];
	glBindTexture(GL_TEXTURE_2D, post_processing->exposure_tex);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixel);
	glBindTexture(GL_TEXTURE_2D, 0);
	return pixel[0]; /* Red channel contains exposure */
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
}

void postprocess_begin(PostProcess* post_processing)
{
	/* Rendre dans notre framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->scene_fbo);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void postprocess_end(PostProcess* post_processing)
{
	/* Générer le bloom (si activé) avant de binder le framebuffer par
	 * défaut */
	render_bloom(post_processing);

	/* Auto Exposure Pass (Compute) - Must be done before binding final
	 * shader */
	if (postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE) &&
	    post_processing->lum_adapt_shader != 0) {
		render_auto_exposure(post_processing);
	}

	/* Retour au framebuffer par défaut */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Désactiver le depth test pour le quad */
	glDisable(GL_DEPTH_TEST);

	/* Utiliser le shader de post-processing */
	glUseProgram(post_processing->postprocess_shader);

	/* Bind la texture de la scène */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "screenTexture"),
	            0);

	/* Bind la texture de Bloom */
	glActiveTexture(GL_TEXTURE1);
	if (postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->bloom_mips[0].texture);
	} else {
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "bloomTexture"),
	            1);

	/* Bind la texture de Profondeur (pour le DoF) */
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_depth_tex);
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "depthTexture"),
	            2);

	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "autoExposureTexture"),
	            3);

	/* Bind Velocity Texture (Unit 4) */
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, post_processing->velocity_tex);
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "velocityTexture"),
	            4);

	/* Envoyer les flags d'effets actifs */
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableVignette"),
	            postprocess_is_enabled(post_processing, POSTFX_VIGNETTE));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableGrain"),
	            postprocess_is_enabled(post_processing, POSTFX_GRAIN));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableExposure"),
	            postprocess_is_enabled(post_processing, POSTFX_EXPOSURE));
	glUniform1i(
	    glGetUniformLocation(post_processing->postprocess_shader,
	                         "enableAutoExposure"),
	    postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE));
	glUniform1i(
	    glGetUniformLocation(post_processing->postprocess_shader,
	                         "enableExposureDebug"),
	    postprocess_is_enabled(post_processing, POSTFX_EXPOSURE_DEBUG));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableChromAbbr"),
	            postprocess_is_enabled(post_processing, POSTFX_CHROM_ABBR));
	glUniform1i(
	    glGetUniformLocation(post_processing->postprocess_shader,
	                         "enableColorGrading"),
	    postprocess_is_enabled(post_processing, POSTFX_COLOR_GRADING));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableBloom"),
	            postprocess_is_enabled(post_processing, POSTFX_BLOOM));

	/* Motion Blur Uniforms */
	glUniform1i(
	    glGetUniformLocation(post_processing->postprocess_shader,
	                         "enableMotionBlur"),
	    postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR));
	glUniform1i(
	    glGetUniformLocation(post_processing->postprocess_shader,
	                         "enableMotionBlurDebug"),
	    postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR_DEBUG));
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "motionBlurIntensity"),
	            1.0f); /* Hardcoded intensity for now, could be in struct */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "motionBlurMaxVelocity"),
	            0.05f); /* Max velocity clamp */
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "motionBlurSamples"),
	            16);
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableDoF"),
	            postprocess_is_enabled(post_processing, POSTFX_DOF));
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableDoFDebug"),
	            postprocess_is_enabled(post_processing, POSTFX_DOF_DEBUG));

	/* Envoyer les paramètres des effets */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "vignetteIntensity"),
	            post_processing->vignette.intensity);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "vignetteExtent"),
	            post_processing->vignette.extent);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "grainIntensity"),
	            post_processing->grain.intensity);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "grainIntensityShadows"),
	            post_processing->grain.intensity_shadows);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "grainIntensityMidtones"),
	            post_processing->grain.intensity_midtones);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "grainIntensityHighlights"),
	            post_processing->grain.intensity_highlights);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "grainShadowsMax"),
	            post_processing->grain.shadows_max);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "grainHighlightsMin"),
	            post_processing->grain.highlights_min);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "grainTexelSize"),
	            post_processing->grain.texel_size);
	glUniform1f(
	    glGetUniformLocation(post_processing->postprocess_shader, "time"),
	    post_processing->time);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "exposure"),
	            post_processing->exposure.exposure);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "chromAbbrStrength"),
	            post_processing->chrom_abbr.strength);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "bloomIntensity"),
	            post_processing->bloom.intensity);

	/* Paramètres DoF */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "dofFocalDistance"),
	            post_processing->dof.focal_distance);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "dofFocalRange"),
	            post_processing->dof.focal_range);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "dofBokehScale"),
	            post_processing->dof.bokeh_scale);

	/* Paramètres Color Grading */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradSaturation"),
	            post_processing->color_grading.saturation);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradContrast"),
	            post_processing->color_grading.contrast);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradGamma"),
	            post_processing->color_grading.gamma);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradGain"),
	            post_processing->color_grading.gain);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "gradOffset"),
	            post_processing->color_grading.offset);

	/* Paramètres White Balance */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "wbTemperature"),
	            post_processing->white_balance.temperature);
	glUniform1f(
	    glGetUniformLocation(post_processing->postprocess_shader, "wbTint"),
	    post_processing->white_balance.tint);

	/* Paramètres Tonemapper */
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "tonemapSlope"),
	            post_processing->tonemapper.slope);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "tonemapToe"),
	            post_processing->tonemapper.toe);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "tonemapShoulder"),
	            post_processing->tonemapper.shoulder);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "tonemapBlackClip"),
	            post_processing->tonemapper.black_clip);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "tonemapWhiteClip"),
	            post_processing->tonemapper.white_clip);

	/* Dessiner le quad */
	glBindVertexArray(post_processing->screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
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

void postprocess_update_matrices(PostProcess* post_processing,
                                 const mat4 view_proj)
{
	/* Save current as previous for next frame */
	glm_mat4_copy((vec4*)view_proj, post_processing->previous_view_proj);
}

static void render_auto_exposure(PostProcess* post_processing)
{
	/* 1. Downsample Scene -> 64x64 Log Luminance */
	glViewport(0, 0, LUM_DOWNSAMPLE_SIZE, LUM_DOWNSAMPLE_SIZE);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->lum_downsample_fbo);

	glUseProgram(post_processing->lum_downsample_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,
	              post_processing->scene_color_tex); /* Input: Scene */
	glUniform1i(glGetUniformLocation(post_processing->lum_downsample_shader,
	                                 "sceneTexture"),
	            0);

	/* Draw Fullscreen Quad (Reuse existing vao) */
	glBindVertexArray(post_processing->screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	glBindVertexArray(0); /* Unbind */

	/* 2. Compute Adaptation */
	glUseProgram(post_processing->lum_adapt_shader);

	/* Input: Downsampled Log Lum */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->lum_downsample_tex);
	/* Note: In compute, samplers map to texture units bound via
	   glActiveTexture? Yes, usually. Or uniform binding. Let's send 0
	   explicitly. */
	glUniform1i(glGetUniformLocation(post_processing->lum_adapt_shader,
	                                 "lumTexture"),
	            0);

	/* Output: Exposure Storage Image (Image Unit 1) */
	glBindImageTexture(1, post_processing->exposure_tex, 0, GL_FALSE, 0,
	                   GL_READ_WRITE, GL_RGBA32F);

	/* Uniforms */
	glUniform1f(glGetUniformLocation(post_processing->lum_adapt_shader,
	                                 "deltaTime"),
	            post_processing->delta_time);
	glUniform1f(glGetUniformLocation(post_processing->lum_adapt_shader,
	                                 "minLuminance"),
	            post_processing->auto_exposure.min_luminance);
	glUniform1f(glGetUniformLocation(post_processing->lum_adapt_shader,
	                                 "maxLuminance"),
	            post_processing->auto_exposure.max_luminance);
	glUniform1f(
	    glGetUniformLocation(post_processing->lum_adapt_shader, "speedUp"),
	    post_processing->auto_exposure.speed_up);
	glUniform1f(glGetUniformLocation(post_processing->lum_adapt_shader,
	                                 "speedDown"),
	            post_processing->auto_exposure.speed_down);
	glUniform1f(
	    glGetUniformLocation(post_processing->lum_adapt_shader, "keyValue"),
	    post_processing->auto_exposure.key_value);

	/* Dispatch (Single Group 1,1,1 as coded in Shader) */
	glDispatchCompute(1, 1, 1);

	/* Memory Barrier to ensure image write is visible to fragment shader
	 * next pass */
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
	                GL_TEXTURE_FETCH_BARRIER_BIT);

	/* Restore Viewport */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, post_processing->width, post_processing->height);
}

/* Fonctions privées */

static int create_framebuffer(PostProcess* post_processing)
{
	/* Créer le framebuffer */
	glGenFramebuffers(1, &post_processing->scene_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->scene_fbo);

	/* Créer la texture de couleur (HDR) */
	glGenTextures(1, &post_processing->scene_color_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, post_processing->width,
	             post_processing->height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, post_processing->scene_color_tex,
	                       0);

	/* Créer la texture de vélocité (GL_RG16F) */
	glGenTextures(1, &post_processing->velocity_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->velocity_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, post_processing->width,
	             post_processing->height, 0, GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
	                       GL_TEXTURE_2D, post_processing->velocity_tex, 0);

	/* Configurer les buffers de rendu (MRT) */
	GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
	glDrawBuffers(2, drawBuffers);

	/* Créer la texture de profondeur (D32F pour précision max) */
	glGenTextures(1, &post_processing->scene_depth_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_depth_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
	             post_processing->width, post_processing->height, 0,
	             GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
	                       GL_TEXTURE_2D, post_processing->scene_depth_tex,
	                       0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Framebuffer incomplete!");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return 0;
	}

	/* --------------------------
	   Auto Exposure Resources
	   -------------------------- */

	/* 1. Downsample Logic (64x64 R16F) */
	glGenFramebuffers(1, &post_processing->lum_downsample_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->lum_downsample_fbo);

	glGenTextures(1, &post_processing->lum_downsample_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->lum_downsample_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, LUM_DOWNSAMPLE_SIZE,
	             LUM_DOWNSAMPLE_SIZE, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D,
	                       post_processing->lum_downsample_tex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Lum Downsample FBO incomplete!");
		return 0;
	}

	/* 2. Adaptation Storage (1x1 RGBA32F) */
	glGenTextures(1, &post_processing->exposure_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->exposure_tex);

	/* Utilisation de GL_RGBA32F pour compatibilité maximale Image
	 * Load/Store */
	/* Init: (0.5, 0, 0, 1) */
	/* Init: (0.5, 0, 0, 1) */
	float initialValues[4] = {EXPOSURE_INITIAL_VAL, 0.0F, 0.0F, 1.0F};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT,
	             initialValues);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 1;
}

static int create_screen_quad(PostProcess* post_processing)
{
	glGenVertexArrays(1, &post_processing->screen_quad_vao);
	glGenBuffers(1, &post_processing->screen_quad_vbo);

	glBindVertexArray(post_processing->screen_quad_vao);
	glBindBuffer(GL_ARRAY_BUFFER, post_processing->screen_quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(screen_quad_vertices),
	             screen_quad_vertices, GL_STATIC_DRAW);

	/* Position */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
	                      (void*)0);

	/* TexCoords */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
	                      BUFFER_OFFSET(2 * sizeof(float)));

	glBindVertexArray(0);

	return 1;
}

static void destroy_framebuffer(PostProcess* post_processing)
{
	if (post_processing->scene_fbo) {
		glDeleteFramebuffers(1, &post_processing->scene_fbo);
		post_processing->scene_fbo = 0;
	}
	if (post_processing->scene_color_tex) {
		glDeleteTextures(1, &post_processing->scene_color_tex);
		post_processing->scene_color_tex = 0;
	}
	if (post_processing->scene_depth_tex) {
		glDeleteTextures(1, &post_processing->scene_depth_tex);
		post_processing->scene_depth_tex = 0;
	}
}

static void destroy_screen_quad(PostProcess* post_processing)
{
	if (post_processing->screen_quad_vao) {
		glDeleteVertexArrays(1, &post_processing->screen_quad_vao);
		post_processing->screen_quad_vao = 0;
	}
	if (post_processing->screen_quad_vbo) {
		glDeleteBuffers(1, &post_processing->screen_quad_vbo);
		post_processing->screen_quad_vbo = 0;
	}
}

static int create_bloom_resources(PostProcess* post_processing)
{
	glGenFramebuffers(1, &post_processing->bloom_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->bloom_fbo);

	int width = post_processing->width;
	int height = post_processing->height;

	for (int i = 0; i < BLOOM_MIP_LEVELS; i++) {
		/* Chaque niveau est la moitié du précédent */
		width /= 2;
		height /= 2;
		if (width < 1) {
			width = 1;
		}
		if (height < 1) {
			height = 1;
		}

		post_processing->bloom_mips[i].width = width;
		post_processing->bloom_mips[i].height = height;

		glGenTextures(1, &post_processing->bloom_mips[i].texture);
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->bloom_mips[i].texture);
		/* R11G11B10F is good for HDR and saves memory vs RGBA16F */
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

static void destroy_bloom_resources(PostProcess* post_processing)
{
	if (post_processing->bloom_fbo) {
		glDeleteFramebuffers(1, &post_processing->bloom_fbo);
		post_processing->bloom_fbo = 0;
	}

	for (int i = 0; i < BLOOM_MIP_LEVELS; i++) {
		if (post_processing->bloom_mips[i].texture) {
			glDeleteTextures(
			    1, &post_processing->bloom_mips[i].texture);
			post_processing->bloom_mips[i].texture = 0;
		}
	}
}

static void render_bloom(PostProcess* post_processing)
{
	if (!postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->bloom_fbo);
	glDisable(GL_DEPTH_TEST);

	/* 1. Prefilter Pass: Extract bright parts from Scene -> Mip 0 */
	glUseProgram(post_processing->bloom_prefilter_shader);
	glUniform1f(glGetUniformLocation(
	                post_processing->bloom_prefilter_shader, "threshold"),
	            post_processing->bloom.threshold);
	glUniform1f(glGetUniformLocation(
	                post_processing->bloom_prefilter_shader, "knee"),
	            post_processing->bloom.soft_threshold);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	glUniform1i(glGetUniformLocation(
	                post_processing->bloom_prefilter_shader, "srcTexture"),
	            0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D,
	                       post_processing->bloom_mips[0].texture, 0);
	glViewport(0, 0, post_processing->bloom_mips[0].width,
	           post_processing->bloom_mips[0].height);

	glBindVertexArray(post_processing->screen_quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);

	/* 2. Downsample Loop */
	glUseProgram(post_processing->bloom_downsample_shader);
	glUniform1i(glGetUniformLocation(
	                post_processing->bloom_downsample_shader, "srcTexture"),
	            0);

	for (int i = 0; i < BLOOM_MIP_LEVELS - 1; i++) {
		const BloomMip* mip_src = &post_processing->bloom_mips[i];
		const BloomMip* mip_dst = &post_processing->bloom_mips[i + 1];

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mip_src->texture);
		glUniform2f(glGetUniformLocation(
		                post_processing->bloom_downsample_shader,
		                "srcResolution"),
		            (float)mip_src->width, (float)mip_src->height);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_2D, mip_dst->texture, 0);
		glViewport(0, 0, mip_dst->width, mip_dst->height);

		glDrawArrays(GL_TRIANGLES, 0, SCREEN_QUAD_VERTEX_COUNT);
	}

	/* 3. Upsample Loop with Blending */
	glUseProgram(post_processing->bloom_upsample_shader);
	glUniform1i(glGetUniformLocation(post_processing->bloom_upsample_shader,
	                                 "srcTexture"),
	            0);
	glUniform1f(glGetUniformLocation(post_processing->bloom_upsample_shader,
	                                 "filterRadius"),
	            post_processing->bloom.radius);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);

	for (int i = BLOOM_MIP_LEVELS - 2; i >= 0; i--) {
		const BloomMip* mip_src = &post_processing->bloom_mips[i + 1];
		const BloomMip* mip_dst = &post_processing->bloom_mips[i];

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
