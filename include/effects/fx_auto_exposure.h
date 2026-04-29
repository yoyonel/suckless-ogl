#ifndef FX_AUTO_EXPOSURE_H
#define FX_AUTO_EXPOSURE_H

#include "gl_common.h"
typedef struct Shader Shader;

/* Forward declarations */
struct EffectContext;

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

/* Downsample render path selection */
typedef enum {
	AE_PATH_FRAGMENT, /* Legacy fullscreen-quad fragment shader */
	AE_PATH_COMPUTE,  /* Compute shader dispatch */
	AE_PATH_COUNT
} AEDownsamplePath;

/* Structure regroupant les ressources graphiques de l'Auto Exposure */
typedef struct {
	/* Common resources */
	GLuint downsample_tex;
	GLuint exposure_tex;
	Shader* adapt_shader;

	/* Fragment path resources */
	GLuint downsample_fbo;
	Shader* downsample_frag_shader;

	/* Compute path resources */
	Shader* downsample_comp_shader;

	/* Active path */
	AEDownsamplePath active_path;
} AutoExposureFX;

/* Initialisation des ressources Auto Exposure */
int fx_auto_exposure_init(AutoExposureFX* auto_exp);

/* Libération des ressources */
void fx_auto_exposure_cleanup(AutoExposureFX* auto_exp);

/* Rendu de l'effet (Downsample + Adaptation) */
void fx_auto_exposure_render(AutoExposureFX* auto_exp,
                             const AutoExposureParams* params,
                             const struct EffectContext* ctx);

/* Récupère la valeur d'exposition actuelle (du GPU) */
float fx_auto_exposure_get_current_exposure(AutoExposureFX* auto_exp);

/* Toggle entre fragment et compute downsample path */
void fx_auto_exposure_toggle_path(AutoExposureFX* auto_exp);

/* Retourne le nom du path actif */
const char* fx_auto_exposure_path_name(AutoExposureFX* auto_exp);

#endif /* FX_AUTO_EXPOSURE_H */
