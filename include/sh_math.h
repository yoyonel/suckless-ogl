#ifndef SH_MATH_H
#define SH_MATH_H

#include <cglm/cglm.h>

/* Ramamoorthi 9 Coefficients for Irradiance Environment Maps */
/* L00, L1-1, L10, L11, L2-2, L2-1, L20, L21, L22 */
/* We use vec4 to align to std430 (16 bytes per coefficient) */
/* The 4th component is padding (or unused) */
typedef struct {
	vec4 coeffs[9];
} SH9;

/**
 * @brief Initialize SH coefficients to zero.
 */
void sh_zero(SH9* sh_ptr);

/**
 * @brief Projects a directional light into SH coefficients.
 * @param dir Normalized direction vector pointing TOWARDS the light source.
 * @param color Linear RGB color/intensity of the light.
 * @param out_sh Pointer to the SH structure to accumulate into.
 */
void sh_project_directional(const vec3 dir, const vec3 color, SH9* out_sh);

/**
 * @brief Evaluates the irradiance from SH coefficients in a given direction.
 * @param normal Normalized surface normal.
 * @param sh_ptr Pointer to the SH structure.
 * @param out_color Output color (vec3).
 */
void sh_eval_irradiance(const vec3 normal, const SH9* sh_ptr, vec3 out_color);

#endif /* SH_MATH_H */
