#ifndef FX_LUT_VIZ_H
#define FX_LUT_VIZ_H

#include "gl_common.h"
#include <stdbool.h>

typedef struct Shader Shader;

/* Forward declarations */
struct EffectContext;

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
int fx_lut_viz_init(LUTVizFX* viz);

/**
 * @brief Releases GPU resources.
 */
void fx_lut_viz_cleanup(LUTVizFX* viz);

/**
 * @brief Renders the LUT lattice.
 * @param viz      LUT visualization resources.
 * @param lut3d_tex OpenGL 3D texture handle (0 = skip).
 * @param ctx      Pipeline context (width/height).
 */
void fx_lut_viz_render(LUTVizFX* viz, GLuint lut3d_tex,
                       const struct EffectContext* ctx);

#endif /* FX_LUT_VIZ_H */
