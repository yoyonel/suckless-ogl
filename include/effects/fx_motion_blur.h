#ifndef FX_MOTION_BLUR_H
#define FX_MOTION_BLUR_H

#include "gl_common.h"
#include "shader.h"
#include <cglm/types.h>

/* Forward declaration */
struct PostProcess;

/* Paramètres pour le Motion Blur */
typedef struct {
	float intensity;
	float max_velocity;
	int samples;
} MotionBlurParams;

/* Structure regroupant les ressources graphiques du Motion Blur */
typedef struct {
	GLuint tile_max_tex;
	GLuint neighbor_max_tex;
	Shader* tile_max_shader;
	Shader* neighbor_max_shader;
	mat4 previous_view_proj;
} MotionBlurFX;

/* Initialisation des ressources Motion Blur */
int fx_motion_blur_init(struct PostProcess* post_processing);

/* Libération des ressources */
void fx_motion_blur_cleanup(struct PostProcess* post_processing);

/* Redimensionnement des textures (tile/neighbor max) */
int fx_motion_blur_resize(struct PostProcess* post_processing);

/* Rendu de l'effet (Passes Compute pour Tile/Neighbor Max) */
void fx_motion_blur_render(struct PostProcess* post_processing);

/* Mise à jour de la matrice de vue-projection précédente */
void fx_motion_blur_update_matrices(struct PostProcess* post_processing,
                                    mat4 view_proj);

/* Envoi des paramètres au shader final */
void fx_motion_blur_upload_params(Shader* shader,
                                  const MotionBlurParams* params);

#endif /* FX_MOTION_BLUR_H */
