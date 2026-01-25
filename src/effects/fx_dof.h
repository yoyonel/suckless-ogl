#ifndef FX_DOF_H
#define FX_DOF_H

#include "gl_common.h"
#include "shader.h"

/* Forward declaration */
struct PostProcess;

/* Paramètres pour le Depth of Field */
typedef struct {
	float focal_distance; /* Distance de mise au point (unités monde) */
	float focal_range; /* Plage de netteté (autour de la distance focale) */
	float bokeh_scale; /* Taille du flou (simule l'ouverture) */
} DoFParams;

/* Structure regroupant les ressources graphiques du DoF */
typedef struct {
	GLuint fbo;
	GLuint blur_tex; /* 1/4 Res Blurred Texture (Final) */
	GLuint temp_tex; /* 1/4 Res Intermediate Texture (Ping-Pong) */
} DoFFX;

/* Initialisation des ressources DoF */
int fx_dof_init(struct PostProcess* post_processing);

/* Libération des ressources */
void fx_dof_cleanup(struct PostProcess* post_processing);

/* Redimensionnement des ressources */
int fx_dof_resize(struct PostProcess* post_processing);

/* Rendu de l'effet */
void fx_dof_render(struct PostProcess* post_processing);

/* Upload des paramètres vers le shader principal */
void fx_dof_upload_params(Shader* shader, const DoFParams* params);

#endif /* FX_DOF_H */
