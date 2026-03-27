#ifndef FX_LUT_VIZ_H
#define FX_LUT_VIZ_H

#include "gl_common.h"
#include "shader.h"

struct PostProcess;

/**
 * @struct LUTVizFX
 * @brief Resources for the 3D LUT Lattice visualization.
 */
typedef struct {
	GLuint vao;
	GLuint vbo;
	Shader* shader;
	int grid_size;   /**< Resolution of the debug grid (e.g. 16 or 32). */
	bool is_enabled; /**< Toggle for SHIFT+F10. */
} LUTVizFX;

/**
 * @brief Initializes the LUT visualization resources.
 */
int fx_lut_viz_init(struct PostProcess* post_processing);

/**
 * @brief Releases GPU resources.
 */
void fx_lut_viz_cleanup(struct PostProcess* post_processing);

/**
 * @brief Renders the LUT lattice.
 */
void fx_lut_viz_render(struct PostProcess* post_processing);

#endif /* FX_LUT_VIZ_H */
