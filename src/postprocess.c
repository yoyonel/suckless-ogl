#include "postprocess.h"

#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "gl_common.h"
#include "log.h"
#include "shader.h"
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <string.h>

static int create_framebuffer(PostProcess* post_processing);
static int create_screen_quad(PostProcess* post_processing);
static void destroy_framebuffer(PostProcess* post_processing);
static void destroy_screen_quad(PostProcess* post_processing);
static void render_motion_blur(PostProcess* post_processing);

/* Texture Units */
enum {
	POSTPROCESS_TEX_UNIT_SCENE = 0,
	POSTPROCESS_TEX_UNIT_BLOOM = 1,
	POSTPROCESS_TEX_UNIT_DEPTH = 2,
	POSTPROCESS_TEX_UNIT_EXPOSURE = 3,
	POSTPROCESS_TEX_UNIT_VELOCITY = 4,
	POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX = 5,
	POSTPROCESS_TEX_UNIT_DOF_BLUR = 6
};

/* Compute Shader Constants */
enum { COMPUTE_WORK_GROUP_SIZE = 16 };

/* Motion Blur Constants */
static const float MB_INTENSITY = 1.0F;
static const float MB_MAX_VELOCITY = 0.05F;
static const int MB_SAMPLES = 8;

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

	post_processing->color_grading.offset = 0.0F;

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
	if (!fx_bloom_init(post_processing)) {
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
		fx_bloom_cleanup(post_processing);
		return 0;
	}

	/* Charger le shader de post-processing */
	post_processing->postprocess_shader =
	    shader_load("shaders/postprocess.vert", "shaders/postprocess.frag");

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

	if (!post_processing->tile_max_shader ||
	    !post_processing->neighbor_max_shader) {
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
		shader_destroy(post_processing->postprocess_shader);
		post_processing->postprocess_shader = NULL;
	}
	fx_bloom_cleanup(post_processing);
	fx_dof_cleanup(post_processing);
	fx_auto_exposure_cleanup(post_processing);

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

	fx_bloom_cleanup(post_processing);
	if (!fx_bloom_init(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to resize bloom resources");
	}

	fx_dof_resize(post_processing);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
                              float smoothness, float roundness)
{
	post_processing->vignette.intensity = intensity;
	post_processing->vignette.smoothness = smoothness;
	post_processing->vignette.roundness = roundness;
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

static void upload_vignette_params(Shader* shader, const VignetteParams* params)
{
	shader_set_float(shader, "vignette.intensity", params->intensity);
	shader_set_float(shader, "vignette.smoothness", params->smoothness);
	shader_set_float(shader, "vignette.roundness", params->roundness);
}

static void upload_grain_params(Shader* shader, const GrainParams* params,
                                float time)
{
	shader_set_float(shader, "grain.intensity", params->intensity);
	shader_set_float(shader, "grain.intensityShadows",
	                 params->intensity_shadows);
	shader_set_float(shader, "grain.intensityMidtones",
	                 params->intensity_midtones);
	shader_set_float(shader, "grain.intensityHighlights",
	                 params->intensity_highlights);
	shader_set_float(shader, "grain.shadowsMax", params->shadows_max);
	shader_set_float(shader, "grain.highlightsMin", params->highlights_min);
	shader_set_float(shader, "grain.texelSize", params->texel_size);
	shader_set_float(shader, "time", time);
}

static void upload_exposure_params(Shader* shader, const ExposureParams* params)
{
	shader_set_float(shader, "exposure.exposure", params->exposure);
}

static void upload_chrom_abbr_params(Shader* shader,
                                     const ChromAbberationParams* params)
{
	shader_set_float(shader, "chromAbbr.strength", params->strength);
}

static void upload_bloom_params(Shader* shader, const BloomParams* params)
{
	fx_bloom_upload_params(shader, params);
}

static void upload_dof_params(Shader* shader, const DoFParams* params)
{
	fx_dof_upload_params(shader, params);
}

static void upload_grading_params(Shader* shader,
                                  const ColorGradingParams* params)
{
	shader_set_float(shader, "grad.saturation", params->saturation);
	shader_set_float(shader, "grad.contrast", params->contrast);
	shader_set_float(shader, "grad.gamma", params->gamma);
	shader_set_float(shader, "grad.gain", params->gain);
	shader_set_float(shader, "grad.offset", params->offset);
}

static void upload_white_balance_params(Shader* shader,
                                        const WhiteBalanceParams* params)
{
	shader_set_float(shader, "wb.temperature", params->temperature);
	shader_set_float(shader, "wb.tint", params->tint);
}

static void upload_tonemap_params(Shader* shader, const TonemapParams* params)
{
	shader_set_float(shader, "tonemap.slope", params->slope);
	shader_set_float(shader, "tonemap.toe", params->toe);
	shader_set_float(shader, "tonemap.shoulder", params->shoulder);
	shader_set_float(shader, "tonemap.blackClip", params->black_clip);
	shader_set_float(shader, "tonemap.whiteClip", params->white_clip);
}

static void upload_motion_blur_params(Shader* shader, float intensity,
                                      float max_v, int samples)
{
	shader_set_float(shader, "motionBlur.intensity", intensity);
	shader_set_float(shader, "motionBlur.maxVelocity", max_v);
	shader_set_int(shader, "motionBlur.samples", samples);
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
	fx_bloom_render(post_processing);

	/* DoF Blur Pass (if DoF enabled) */
	/* We reuse bloom_downsample to get a filtered 1/2 res version of the
	 * scene */
	if (postprocess_is_enabled(post_processing, POSTFX_DOF) ||
	    postprocess_is_enabled(post_processing, POSTFX_DOF_DEBUG)) {
		fx_dof_render(post_processing);
	}

	/* Auto Exposure Pass */
	if (postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE)) {
		fx_auto_exposure_render(post_processing);
	}

	/* Motion Blur Pre-Pass (Compute) */
	if (postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR) &&
	    post_processing->tile_max_shader != 0) {
		render_motion_blur(post_processing);
	}

	/* Retour au framebuffer par défaut */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, post_processing->width, post_processing->height);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Désactiver le depth test pour le quad */
	glDisable(GL_DEPTH_TEST);

	/* Utiliser le shader de post-processing */
	shader_use(post_processing->postprocess_shader);

	/* Bind la texture de la scène */
	glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_SCENE);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	shader_set_int(post_processing->postprocess_shader, "screenTexture",
	               POSTPROCESS_TEX_UNIT_SCENE);

	/* Bind la texture de Bloom */
	glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_BLOOM);
	if (postprocess_is_enabled(post_processing, POSTFX_BLOOM)) {
		glBindTexture(GL_TEXTURE_2D,
		              post_processing->bloom_fx.mips[0].texture);
	} else {
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	shader_set_int(post_processing->postprocess_shader, "bloomTexture",
	               POSTPROCESS_TEX_UNIT_BLOOM);

	/* Bind la texture de Profondeur (pour le DoF) */
	glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_DEPTH);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_depth_tex);
	shader_set_int(post_processing->postprocess_shader, "depthTexture",
	               POSTPROCESS_TEX_UNIT_DEPTH);

	/* Bind Exposure Texture (Unit 3) */
	glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_EXPOSURE);
	glBindTexture(GL_TEXTURE_2D,
	              post_processing->auto_exposure_fx.exposure_tex);
	shader_set_int(post_processing->postprocess_shader,
	               "autoExposureTexture", POSTPROCESS_TEX_UNIT_EXPOSURE);

	/* Bind Velocity Texture (Unit 4) */
	glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_VELOCITY);
	glBindTexture(GL_TEXTURE_2D, post_processing->velocity_tex);
	shader_set_int(post_processing->postprocess_shader, "velocityTexture",
	               POSTPROCESS_TEX_UNIT_VELOCITY);

	/* Bind Neighbor Max Texture (Unit 5) */
	glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX);
	glBindTexture(GL_TEXTURE_2D, post_processing->neighbor_max_tex);
	shader_set_int(post_processing->postprocess_shader,
	               "neighborMaxTexture", POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX);

	/* Bind DoF Blurred Texture (Unit 6) */
	glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_DOF_BLUR);
	glBindTexture(GL_TEXTURE_2D, post_processing->dof_fx.blur_tex);
	shader_set_int(post_processing->postprocess_shader, "dofBlurTexture",
	               POSTPROCESS_TEX_UNIT_DOF_BLUR);

	/* Envoyer les flags d'effets actifs */
	shader_set_int(
	    post_processing->postprocess_shader, "enableVignette",
	    postprocess_is_enabled(post_processing, POSTFX_VIGNETTE));
	shader_set_int(post_processing->postprocess_shader, "enableGrain",
	               postprocess_is_enabled(post_processing, POSTFX_GRAIN));
	shader_set_int(
	    post_processing->postprocess_shader, "enableExposure",
	    postprocess_is_enabled(post_processing, POSTFX_EXPOSURE));
	shader_set_int(
	    post_processing->postprocess_shader, "enableAutoExposure",
	    postprocess_is_enabled(post_processing, POSTFX_AUTO_EXPOSURE));
	shader_set_int(
	    post_processing->postprocess_shader, "enableChromAbbr",
	    postprocess_is_enabled(post_processing, POSTFX_CHROM_ABBR));
	shader_set_int(
	    post_processing->postprocess_shader, "enableColorGrading",
	    postprocess_is_enabled(post_processing, POSTFX_COLOR_GRADING));
	shader_set_int(post_processing->postprocess_shader, "enableBloom",
	               postprocess_is_enabled(post_processing, POSTFX_BLOOM));

	/* Motion Blur Flags & Uniforms */
	shader_set_int(
	    post_processing->postprocess_shader, "enableMotionBlur",
	    postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR));
	shader_set_int(
	    post_processing->postprocess_shader, "enableMotionBlurDebug",
	    postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR_DEBUG));

	upload_motion_blur_params(post_processing->postprocess_shader,
	                          MB_INTENSITY, MB_MAX_VELOCITY, MB_SAMPLES);

	/* DoF Flags */
	shader_set_int(post_processing->postprocess_shader, "enableDoF",
	               postprocess_is_enabled(post_processing, POSTFX_DOF));
	shader_set_int(
	    post_processing->postprocess_shader, "enableDoFDebug",
	    postprocess_is_enabled(post_processing, POSTFX_DOF_DEBUG));

	/* Envoyer les paramètres via helpers (High Habitability) */
	upload_vignette_params(post_processing->postprocess_shader,
	                       &post_processing->vignette);

	upload_grain_params(post_processing->postprocess_shader,
	                    &post_processing->grain, post_processing->time);

	upload_exposure_params(post_processing->postprocess_shader,
	                       &post_processing->exposure);

	upload_chrom_abbr_params(post_processing->postprocess_shader,
	                         &post_processing->chrom_abbr);

	upload_bloom_params(post_processing->postprocess_shader,
	                    &post_processing->bloom);

	upload_dof_params(post_processing->postprocess_shader,
	                  &post_processing->dof);

	upload_grading_params(post_processing->postprocess_shader,
	                      &post_processing->color_grading);

	upload_white_balance_params(post_processing->postprocess_shader,
	                            &post_processing->white_balance);

	upload_tonemap_params(post_processing->postprocess_shader,
	                      &post_processing->tonemapper);

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

void postprocess_update_matrices(PostProcess* post_processing, mat4 view_proj)
{
	/* Save current as previous for next frame */
	glm_mat4_copy(view_proj, post_processing->previous_view_proj);
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

	/* --------------------------
	   Motion Blur Resources (McGuire)
	   -------------------------- */
	int tile_width =
	    (post_processing->width + (COMPUTE_WORK_GROUP_SIZE - 1)) /
	    COMPUTE_WORK_GROUP_SIZE;
	int tile_height =
	    (post_processing->height + (COMPUTE_WORK_GROUP_SIZE - 1)) /
	    COMPUTE_WORK_GROUP_SIZE;
	if (tile_width < 1) {
		tile_width = 1;
	}
	if (tile_height < 1) {
		tile_height = 1;
	}

	/* Tile Max Texture (RG16F) */
	glGenTextures(1, &post_processing->tile_max_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->tile_max_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, tile_width, tile_height, 0,
	             GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	/* Neighbor Max Texture (RG16F) */
	glGenTextures(1, &post_processing->neighbor_max_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->neighbor_max_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, tile_width, tile_height, 0,
	             GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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

	return 1;

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
	if (post_processing->tile_max_tex) {
		glDeleteTextures(1, &post_processing->tile_max_tex);
		post_processing->tile_max_tex = 0;
	}
	if (post_processing->neighbor_max_tex) {
		glDeleteTextures(1, &post_processing->neighbor_max_tex);
		post_processing->neighbor_max_tex = 0;
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

static void render_motion_blur(PostProcess* post_processing)
{
	int groups_x =
	    (post_processing->width + (COMPUTE_WORK_GROUP_SIZE - 1)) /
	    COMPUTE_WORK_GROUP_SIZE;
	int groups_y =
	    (post_processing->height + (COMPUTE_WORK_GROUP_SIZE - 1)) /
	    COMPUTE_WORK_GROUP_SIZE;

	/* Pass 1: Tile Max Velocity */
	shader_use(post_processing->tile_max_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->velocity_tex);
	shader_set_int(post_processing->tile_max_shader, "velocityTexture", 0);

	glBindImageTexture(1, post_processing->tile_max_tex, 0, GL_FALSE, 0,
	                   GL_WRITE_ONLY, GL_RG16F);

	glDispatchCompute(groups_x, groups_y, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
	                GL_TEXTURE_FETCH_BARRIER_BIT);

	/* Pass 2: Neighbor Max Velocity */
	shader_use(post_processing->neighbor_max_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->tile_max_tex);
	shader_set_int(post_processing->neighbor_max_shader, "tileMaxTexture",
	               0);

	glBindImageTexture(1, post_processing->neighbor_max_tex, 0, GL_FALSE, 0,
	                   GL_WRITE_ONLY, GL_RG16F);

	glDispatchCompute(groups_x, groups_y, 1);
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}
