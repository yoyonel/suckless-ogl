#include "postprocess.h"

#include "effects/fx_auto_exposure.h"
#include "effects/fx_bloom.h"
#include "effects/fx_dof.h"
#include "effects/fx_motion_blur.h"
#include "gl_common.h"
#include "log.h"
#include "shader.h"
#include <cglm/types.h>
#include <string.h>

static int create_framebuffer(PostProcess* post_processing);
static int create_screen_quad(PostProcess* post_processing);
static void destroy_framebuffer(PostProcess* post_processing);
static void destroy_screen_quad(PostProcess* post_processing);

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
enum { POSTPROCESS_COMPUTE_GROUP_SIZE = 16 };

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

	/* Initialisation Motion Blur */
	if (!fx_motion_blur_init(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create motion blur resources");
		/* On continue quand même */
	}

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

void postprocess_cleanup(PostProcess* post_processing)
{
	destroy_framebuffer(post_processing);
	destroy_screen_quad(post_processing);

	if (post_processing->settings_ubo) {
		glDeleteBuffers(1, &post_processing->settings_ubo);
		post_processing->settings_ubo = 0;
	}

	if (post_processing->postprocess_shader) {
		shader_destroy(post_processing->postprocess_shader);
		post_processing->postprocess_shader = NULL;
	}
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
	for (int i = 0; i <= POSTPROCESS_TEX_UNIT_DOF_BLUR; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, post_processing->dummy_black_tex);
	}

	/* Reset to Unit 0 for subsequent generic bindings */
	glActiveTexture(GL_TEXTURE0);

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
	if (postprocess_is_enabled(post_processing, POSTFX_MOTION_BLUR)) {
		fx_motion_blur_render(post_processing);
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
		glBindTexture(GL_TEXTURE_2D, post_processing->dummy_black_tex);
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
	glBindTexture(GL_TEXTURE_2D,
	              post_processing->motion_blur_fx.neighbor_max_tex);
	shader_set_int(post_processing->postprocess_shader,
	               "neighborMaxTexture", POSTPROCESS_TEX_UNIT_NEIGHBOR_MAX);

	/* Bind DoF Blurred Texture (Unit 6) */
	glActiveTexture(GL_TEXTURE0 + POSTPROCESS_TEX_UNIT_DOF_BLUR);
	glBindTexture(GL_TEXTURE_2D, post_processing->dof_fx.blur_tex);
	shader_set_int(post_processing->postprocess_shader, "dofBlurTexture",
	               POSTPROCESS_TEX_UNIT_DOF_BLUR);

	/* Upload settings via UBO */
	PostProcessUBO ubo = {0};
	ubo.active_effects = post_processing->active_effects;
	ubo.time = post_processing->time;

	ubo.vignette_intensity = post_processing->vignette.intensity;
	ubo.vignette_smoothness = post_processing->vignette.smoothness;
	ubo.vignette_roundness = post_processing->vignette.roundness;

	ubo.grain_intensity = post_processing->grain.intensity;
	ubo.grain_intensity_shadows = post_processing->grain.intensity_shadows;
	ubo.grain_intensity_midtones =
	    post_processing->grain.intensity_midtones;
	ubo.grain_intensity_highlights =
	    post_processing->grain.intensity_highlights;
	ubo.grain_shadows_max = post_processing->grain.shadows_max;
	ubo.grain_highlights_min = post_processing->grain.highlights_min;
	ubo.grain_texel_size = post_processing->grain.texel_size;

	ubo.exposure_manual = post_processing->exposure.exposure;
	ubo.chrom_abbr_strength = post_processing->chrom_abbr.strength;

	ubo.wb_temperature = post_processing->white_balance.temperature;
	ubo.wb_tint = post_processing->white_balance.tint;

	ubo.grading_saturation = post_processing->color_grading.saturation;
	ubo.grading_contrast = post_processing->color_grading.contrast;
	ubo.grading_gamma = post_processing->color_grading.gamma;
	ubo.grading_gain = post_processing->color_grading.gain;
	ubo.grading_offset = post_processing->color_grading.offset;

	ubo.tonemap_slope = post_processing->tonemapper.slope;
	ubo.tonemap_toe = post_processing->tonemapper.toe;
	ubo.tonemap_shoulder = post_processing->tonemapper.shoulder;
	ubo.tonemap_black_clip = post_processing->tonemapper.black_clip;
	ubo.tonemap_white_clip = post_processing->tonemapper.white_clip;

	ubo.bloom_intensity = post_processing->bloom.intensity;
	ubo.bloom_threshold = post_processing->bloom.threshold;
	ubo.bloom_soft_threshold = post_processing->bloom.soft_threshold;
	ubo.bloom_radius = post_processing->bloom.radius;

	ubo.dof_focal_distance = post_processing->dof.focal_distance;
	ubo.dof_focal_range = post_processing->dof.focal_range;
	ubo.dof_bokeh_scale = post_processing->dof.bokeh_scale;

	ubo.mb_intensity = post_processing->motion_blur.intensity;
	ubo.mb_max_velocity = post_processing->motion_blur.max_velocity;
	ubo.mb_samples = post_processing->motion_blur.samples;

	glBindBuffer(GL_UNIFORM_BUFFER, post_processing->settings_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PostProcessUBO), &ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

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
	fx_motion_blur_update_matrices(post_processing, view_proj);
}

/* Fonctions privées */

static int create_framebuffer(PostProcess* post_processing)
{
	/* Ensure Unit 0 is active for initial texture setup */
	glActiveTexture(GL_TEXTURE0);

	/* Créer le framebuffer */
	glGenFramebuffers(1, &post_processing->scene_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->scene_fbo);

	/* Créer la texture de couleur (HDR) */
	glGenTextures(1, &post_processing->scene_color_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_color_tex);
	glObjectLabel(GL_TEXTURE, post_processing->scene_color_tex, -1,
	              "Scene Color (HDR)");
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
	glObjectLabel(GL_TEXTURE, post_processing->velocity_tex, -1,
	              "Velocity Buffer");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, post_processing->width,
	             post_processing->height, 0, GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
	                       GL_TEXTURE_2D, post_processing->velocity_tex, 0);

	/* Neighbor Max Texture (RG16F) */

	/* Configurer les buffers de rendu (MRT) */
	GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
	glDrawBuffers(2, drawBuffers);

	/* Créer la texture de profondeur (D32F pour précision max) */
	glGenTextures(1, &post_processing->scene_depth_tex);
	glBindTexture(GL_TEXTURE_2D, post_processing->scene_depth_tex);
	glObjectLabel(GL_TEXTURE, post_processing->scene_depth_tex, -1,
	              "Scene Depth (D32F)");
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
	glVertexAttribDivisor(0, 0);

	/* TexCoords */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
	                      BUFFER_OFFSET(2 * sizeof(float)));
	glVertexAttribDivisor(1, 0);

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
	if (post_processing->velocity_tex) {
		glDeleteTextures(1, &post_processing->velocity_tex);
		post_processing->velocity_tex = 0;
	}

	/* Bridge Unit 0 with dummy to avoid invalid state warnings during
	 * resize
	 */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, post_processing->dummy_black_tex);
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
