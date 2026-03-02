// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
#include "sh_math.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Constants for SH Basis Functions (Y_lm) */
/* Band 0 */
#define Y00 0.28209479177387814347f /* 0.5 * sqrt(1/pi) */

/* Band 1 */
#define Y1n1 0.48860251190291992159f /* -sqrt(3/4pi) * y */
#define Y10 0.48860251190291992159f  /* sqrt(3/4pi) * z */
#define Y11 0.48860251190291992159f  /* -sqrt(3/4pi) * x */

/* Band 2 */
#define Y2n2 1.09254843059207907054f /* 0.5 * sqrt(15/pi) * xy */
#define Y2n1 1.09254843059207907054f /* -0.5 * sqrt(15/pi) * yz */
#define Y20 0.31539156525251999825f  /* 0.25 * sqrt(5/pi) * (3z^2 - 1) */
#define Y21 1.09254843059207907054f  /* -0.5 * sqrt(15/pi) * xz */
#define Y22 0.54627421529603953527f  /* 0.25 * sqrt(15/pi) * (x^2 - y^2) */

/* Cosine Convolution Coefficients (A_l) for Irradiance */
#define A0 3.14159265359f /* PI */
#define A1 2.09439510239f /* 2PI/3 */
#define A2 0.78539816339f /* PI/4 */

void sh_zero(SH9* sh_ptr)
{
	if (!sh_ptr) {
		return;
	}
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	memset(sh_ptr, 0, sizeof(SH9));
}

void sh_project_directional(const vec3 dir, const vec3 color, SH9* out_sh)
{
	if (!out_sh) {
		return;
	}

	float dir_x = dir[0];
	float dir_y = dir[1];
	float dir_z = dir[2];

	/* SH Basis evaluation */
	float L00 = Y00;
	float L1n1 = Y1n1 * dir_y;
	float L10 = Y10 * dir_z;
	float L11 = Y11 * dir_x;
	float L2n2 = Y2n2 * dir_x * dir_y;
	float L2n1 = Y2n1 * dir_y * dir_z;
	float L20 = Y20 * ((3.0F * dir_z * dir_z) - 1.0F);
	float L21 = Y21 * dir_x * dir_z;
	float L22 = Y22 * ((dir_x * dir_x) - (dir_y * dir_y));

	/* Accumulate into coefficients */
	/* We multiply by the light color */

	/* Band 0 */
	glm_vec3_muladds((float*)color, L00, out_sh->coeffs[0]);

	/* Band 1 */
	glm_vec3_muladds((float*)color, L1n1, out_sh->coeffs[1]);
	glm_vec3_muladds((float*)color, L10, out_sh->coeffs[2]);
	glm_vec3_muladds((float*)color, L11, out_sh->coeffs[3]);

	/* Band 2 */
	glm_vec3_muladds((float*)color, L2n2, out_sh->coeffs[4]);
	glm_vec3_muladds((float*)color, L2n1, out_sh->coeffs[5]);
	glm_vec3_muladds((float*)color, L20, out_sh->coeffs[6]);
	glm_vec3_muladds((float*)color, L21, out_sh->coeffs[7]);
	glm_vec3_muladds((float*)color, L22, out_sh->coeffs[8]);
}

void sh_eval_irradiance(const vec3 normal, const SH9* sh_ptr, vec3 out_color)
{
	if (!sh_ptr || !out_color) {
		return;
	}

	float norm_x = normal[0];
	float norm_y = normal[1];
	float norm_z = normal[2];

	/* Re-evaluate basis functions in normal direction */
	float c00 = Y00;
	float c1n1 = Y1n1 * norm_y;
	float c10 = Y10 * norm_z;
	float c11 = Y11 * norm_x;
	float c2n2 = Y2n2 * norm_x * norm_y;
	float c2n1 = Y2n1 * norm_y * norm_z;
	float c20 = Y20 * ((3.0F * norm_z * norm_z) - 1.0F);
	float c21 = Y21 * norm_x * norm_z;
	float c22 = Y22 * ((norm_x * norm_x) - (norm_y * norm_y));

	/* Apply Cosine Convolution Coefficients (A_l) */
	/* Irradiance E(n) = sum(A_l * L_lm * Y_lm(n)) */

	/* Initialize with Band 0 */
	/* Cast to (vec3) (float*) to avoid const warnings and type mismatches
	 */
	glm_vec3_scale((float*)sh_ptr->coeffs[0], A0 * c00, out_color);

	/* Band 1 */
	glm_vec3_muladds((float*)sh_ptr->coeffs[1], A1 * c1n1, out_color);
	glm_vec3_muladds((float*)sh_ptr->coeffs[2], A1 * c10, out_color);
	glm_vec3_muladds((float*)sh_ptr->coeffs[3], A1 * c11, out_color);

	/* Band 2 */
	glm_vec3_muladds((float*)sh_ptr->coeffs[4], A2 * c2n2, out_color);
	glm_vec3_muladds((float*)sh_ptr->coeffs[5], A2 * c2n1, out_color);
	glm_vec3_muladds((float*)sh_ptr->coeffs[6], A2 * c20, out_color);
	glm_vec3_muladds((float*)sh_ptr->coeffs[7], A2 * c21, out_color);
	glm_vec3_muladds((float*)sh_ptr->coeffs[8], A2 * c22, out_color);
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
