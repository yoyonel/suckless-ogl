#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "gl_common.h"
#include <cglm/cglm.h>

/* Valeurs par défaut - plus subtiles et cinématiques */
#define DEFAULT_VIGNETTE_INTENSITY 0.03F /* 0.5 était trop fort */
#define DEFAULT_VIGNETTE_EXTENT 0.7F     /* Plus large = effet plus doux */
#define DEFAULT_GRAIN_INTENSITY 0.02F    /* 0.05 était trop visible */
#define DEFAULT_EXPOSURE 1.00F
#define DEFAULT_CHROM_ABBR_STRENGTH 0.01F /* x3 pour le rendre visible */

/* Types d'effets de post-traitement disponibles */
typedef enum {
	POSTFX_VIGNETTE = (1 << 0),   /* 0x01 */
	POSTFX_GRAIN = (1 << 1),      /* 0x02 */
	POSTFX_EXPOSURE = (1 << 2),   /* 0x04 */
	POSTFX_CHROM_ABBR = (1 << 3), /* 0x08 */
	/* Réservé pour futurs effets */
	POSTFX_BLOOM = (1 << 4), /* 0x10 */
} PostProcessEffect;

/* Paramètres pour le vignettage */
typedef struct {
	float intensity; /* 0.0 - 1.0, défaut: 0.3, recommandé: 0.2-0.4 (subtil)
	                  */
	float extent; /* 0.3 - 0.8, défaut: 0.7, recommandé: 0.6-0.8 (doux) */
} VignetteParams;

/* Paramètres pour le grain */
typedef struct {
	float intensity; /* 0.0 - 0.1, défaut: 0.02, recommandé: 0.01-0.03
	                    (film) */
} GrainParams;

/* Paramètres pour l'exposition */
typedef struct {
	float exposure; /* 0.5 - 2.0, défaut: 1.0, recommandé: 0.8-1.5 */
} ExposureParams;

/* Paramètres pour l'aberration chromatique */
typedef struct {
	float strength; /* 0.0 - 0.05, défaut: 0.01, recommandé: 0.005-0.02
	                   (visible aux bords) */
} ChromAbberationParams;

/* Structure principale du système de post-processing */
typedef struct {
	/* Framebuffers */
	GLuint scene_fbo;       /* FBO pour le rendu de la scène */
	GLuint scene_color_tex; /* Texture de couleur HDR */
	GLuint scene_depth_rbo; /* Renderbuffer pour depth/stencil */

	/* Quad plein écran */
	GLuint screen_quad_vao;
	GLuint screen_quad_vbo;

	/* Shaders */
	GLuint postprocess_shader; /* Shader combinant tous les effets */

	/* Dimensions */
	int width;
	int height;

	/* Effets actifs (masque de bits) */
	unsigned int active_effects;

	/* Paramètres des effets */
	VignetteParams vignette;
	GrainParams grain;
	ExposureParams exposure;
	ChromAbberationParams chrom_abbr;

	/* Temps pour effets animés (grain) */
	float time;
} PostProcess;

/* Initialisation et nettoyage */
int postprocess_init(PostProcess* post_processing, int width, int height);
void postprocess_cleanup(PostProcess* post_processing);

/* Redimensionnement */
void postprocess_resize(PostProcess* post_processing, int width, int height);

/* Activation/désactivation d'effets */
void postprocess_enable(PostProcess* post_processing, PostProcessEffect effect);
void postprocess_disable(PostProcess* post_processing,
                         PostProcessEffect effect);
void postprocess_toggle(PostProcess* post_processing, PostProcessEffect effect);
int postprocess_is_enabled(PostProcess* post_processing,
                           PostProcessEffect effect);

/* Configuration des paramètres */
void postprocess_set_vignette(PostProcess* post_processing, float intensity,
                              float extent);
void postprocess_set_grain(PostProcess* post_processing, float intensity);
void postprocess_set_exposure(PostProcess* post_processing, float exposure);
void postprocess_set_chrom_abbr(PostProcess* post_processing, float strength);

/* Rendu */
void postprocess_begin(
    PostProcess* post_processing); /* Commence le rendu dans le FBO */
void postprocess_end(
    PostProcess* post_processing); /* Applique les effets et rend à l'écran */
void postprocess_update_time(PostProcess* post_processing, float delta_time);

#endif /* POSTPROCESS_H */