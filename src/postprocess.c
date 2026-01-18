#include "postprocess.h"

#include "gl_common.h"
#include "log.h"
#include "shader.h"
#include <stdlib.h>
#include <string.h>

static int create_framebuffer(PostProcess* post_processing);
static int create_screen_quad(PostProcess* post_processing);
static void destroy_framebuffer(PostProcess* post_processing);
static void destroy_screen_quad(PostProcess* post_processing);

/* TODO: move to gl_common.h and mutualize with skybox rendering */
enum { SCREEN_QUAD_VERTEX_COUNT = 6 };

/* Vertices pour un quad plein écran */
static const float screen_quad_vertices[SCREEN_QUAD_VERTEX_COUNT * (2 + 2)] =
    {/* positions     texCoords */
     -1.0F, 1.0F, 0.0F, 1.0F,  -1.0F, -1.0F,
     0.0F,  0.0F, 1.0F, -1.0F, 1.0F,  0.0F,

     -1.0F, 1.0F, 0.0F, 1.0F,  1.0F,  -1.0F,
     1.0F,  0.0F, 1.0F, 1.0F,  1.0F,  1.0F};

int postprocess_init(PostProcess* post_processing, int width, int height)
{
	memset(post_processing, 0, sizeof(PostProcess));

	post_processing->width = width;
	post_processing->height = height;
	post_processing->time = 0.0F;

	/* Paramètres par défaut */
	post_processing->vignette.intensity = DEFAULT_VIGNETTE_INTENSITY;
	post_processing->vignette.extent = DEFAULT_VIGNETTE_EXTENT;
	post_processing->grain.intensity = DEFAULT_GRAIN_INTENSITY;
	post_processing->exposure.exposure = DEFAULT_EXPOSURE;
	post_processing->chrom_abbr.strength = DEFAULT_CHROM_ABBR_STRENGTH;

	/* Initialisation Color Grading (Neutre) */
	post_processing->color_grading.saturation = 1.0F;
	post_processing->color_grading.contrast = 1.0F;
	post_processing->color_grading.gamma = 1.0F;
	post_processing->color_grading.gain = 1.0F;
	post_processing->color_grading.offset = 0.0F;

	/* Effets désactivés par défaut */
	post_processing->active_effects = 0;

	/* Créer le framebuffer */
	if (!create_framebuffer(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create framebuffer");
		return 0;
	}

	/* Créer le quad plein écran */
	if (!create_screen_quad(post_processing)) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to create screen quad");
		destroy_framebuffer(post_processing);
		return 0;
	}

	/* Charger le shader de post-processing */
	post_processing->postprocess_shader = shader_load_program(
	    "shaders/postprocess.vert", "shaders/postprocess.frag");

	if (!post_processing->postprocess_shader) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Failed to load postprocess shader");
		destroy_framebuffer(post_processing);
		destroy_screen_quad(post_processing);
		return 0;
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

	LOG_INFO("suckless-ogl.postprocess", "Resized to %dx%d", width, height);
}

void postprocess_enable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects |= effect;
}

void postprocess_disable(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects &= ~effect;
}

void postprocess_toggle(PostProcess* post_processing, PostProcessEffect effect)
{
	post_processing->active_effects ^= effect;
}

int postprocess_is_enabled(PostProcess* post_processing,
                           PostProcessEffect effect)
{
	return (post_processing->active_effects & effect) != 0;
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

void postprocess_begin(PostProcess* post_processing)
{
	/* Rendre dans notre framebuffer */
	glBindFramebuffer(GL_FRAMEBUFFER, post_processing->scene_fbo);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void postprocess_end(PostProcess* post_processing)
{
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
	glUniform1i(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "enableChromAbbr"),
	            postprocess_is_enabled(post_processing, POSTFX_CHROM_ABBR));
	glUniform1i(
	    glGetUniformLocation(post_processing->postprocess_shader,
	                         "enableColorGrading"),
	    postprocess_is_enabled(post_processing, POSTFX_COLOR_GRADING));

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
	glUniform1f(
	    glGetUniformLocation(post_processing->postprocess_shader, "time"),
	    post_processing->time);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "exposure"),
	            post_processing->exposure.exposure);
	glUniform1f(glGetUniformLocation(post_processing->postprocess_shader,
	                                 "chromAbbrStrength"),
	            post_processing->chrom_abbr.strength);
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

	/* Créer le renderbuffer pour depth/stencil */
	glGenRenderbuffers(1, &post_processing->scene_depth_rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, post_processing->scene_depth_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
	                      post_processing->width, post_processing->height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
	                          GL_RENDERBUFFER,
	                          post_processing->scene_depth_rbo);

	/* Vérifier que le framebuffer est complet */
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR("suckless-ogl.postprocess",
		          "Framebuffer incomplete!");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return 0;
	}

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
	if (post_processing->scene_depth_rbo) {
		glDeleteRenderbuffers(1, &post_processing->scene_depth_rbo);
		post_processing->scene_depth_rbo = 0;
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