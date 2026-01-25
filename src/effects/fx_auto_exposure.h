#ifndef FX_AUTO_EXPOSURE_H
#define FX_AUTO_EXPOSURE_H

#include "gl_common.h"
#include "shader.h"

/* Forward declaration */
struct PostProcess;

#define EXPOSURE_MIN_LUM 0.05F
#define EXPOSURE_DEFAULT_MAX_LUM 5000.0F
#define EXPOSURE_SPEED_UP 2.0F
#define EXPOSURE_SPEED_DOWN 1.0F
#define EXPOSURE_DEFAULT_KEY_VALUE 0.20F

/* Paramètres pour l'Auto Exposure (Eye Adaptation) */
typedef struct {
	float min_luminance; /* Luminance min (clamping) - Range param */
	float max_luminance; /* Luminance max (clamping) - Range param */
	float speed_up; /* Vitesse d'adaptation vers clair (pupille s'ouvre) */
	float speed_down; /* Vitesse d'adaptation vers sombre (pupille se ferme)
	                   */
	float key_value;  /* Target exposure value (middle gray), def: 1.0 */
} AutoExposureParams;

/* Structure regroupant les ressources graphiques de l'Auto Exposure */
typedef struct {
	GLuint downsample_fbo;
	GLuint downsample_tex;
	GLuint exposure_tex;
	Shader* downsample_shader;
	Shader* adapt_shader;
} AutoExposureFX;

/* Initialisation des ressources Auto Exposure */
int fx_auto_exposure_init(struct PostProcess* post_processing);

/* Libération des ressources */
void fx_auto_exposure_cleanup(struct PostProcess* post_processing);

/* Rendu de l'effet (Downsample + Adaptation) */
void fx_auto_exposure_render(struct PostProcess* post_processing);

/* Récupère la valeur d'exposition actuelle (du GPU) */
float fx_auto_exposure_get_current_exposure(
    struct PostProcess* post_processing);

#endif /* FX_AUTO_EXPOSURE_H */
