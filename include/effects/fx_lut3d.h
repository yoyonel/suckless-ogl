#ifndef FX_LUT3D_H
#define FX_LUT3D_H

#include "gl_common.h"
#include "shader.h"
#include <stdbool.h>

/* Forward declaration */
struct PostProcess;

/**
 * @struct LUT3DParams
 * @brief Parameters for 3D LUT gamut mapping.
 */
typedef struct {
	float
	    intensity; /**< Blending factor (0.0 = identity, 1.0 = full LUT). */
	GLuint texture; /**< OpenGL 3D texture handle. */
	int size;       /**< Resolution of the LUT (e.g., 32 for 32x32x32). */
} LUT3DParams;

/**
 * @struct LUT3DFX
 * @brief Internal resources for the 3D LUT effect.
 */
typedef struct {
	GLuint lut_tex;   /**< Currently loaded 3D texture. */
	int current_size; /**< Resolution of the loaded LUT. */
} LUT3DFX;

/**
 * @brief Prepares resources for 3D LUT processing.
 * @param post_processing Pointer to main pipeline.
 * @return 0 on success.
 */
int fx_lut3d_init(struct PostProcess* post_processing);

/**
 * @brief Releases GPU resources.
 * @param post_processing Pointer to main pipeline.
 */
void fx_lut3d_cleanup(struct PostProcess* post_processing);

/**
 * @brief Parses a .cube file and uploads it to a 3D texture.
 * @param post_processing Pointer to main pipeline.
 * @param path Path to the Adobe .cube file.
 * @return 0 on success, negative on error.
 */
int fx_lut3d_load_cube(struct PostProcess* post_processing, const char* path);

/**
 * @brief Uploads LUT parameters to the destination shader.
 * @param shader Target shader.
 * @param params Parameters to upload.
 */
void fx_lut3d_upload_params(Shader* shader, const LUT3DParams* params);

#endif /* FX_LUT3D_H */
