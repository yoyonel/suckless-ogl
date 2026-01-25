#ifndef BILLBOARD_RENDERING_H
#define BILLBOARD_RENDERING_H

#include "gl_common.h"
#include "instanced_rendering.h" /* For SphereInstance */

typedef struct {
	GLuint vao;           // VAO dédié (Quad Geometry + Instances)
	GLuint instance_vbo;  // Stockage des instances sur GPU
	int instance_count;   // Nombre de sphères
} BillboardGroup;

/* Alloue le buffer d'instances sur le GPU */
void billboard_group_init(BillboardGroup* group, const SphereInstance* data,
                          int count);

/* Prépare le VAO en liant la geometrie Quad (passée en argument)
   avec le VBO d'instances interne */
void billboard_group_prepare(BillboardGroup* group, GLuint quad_vbo);

/* Dessine les billboards (GL_TRIANGLE_STRIP instancié) */
void billboard_group_draw(BillboardGroup* group);

void billboard_group_cleanup(BillboardGroup* group);

#endif
