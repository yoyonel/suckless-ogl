#include "sh_math.h"
#include <cglm/cglm.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Helper to compare vec3 with tolerance */
int vec3_approx_eq(vec3 a, vec3 b, float tolerance)
{
	float diff[3];
	glm_vec3_sub(a, b, diff);
	float len = glm_vec3_norm(diff);
	if (len > tolerance) {
		printf(
		    "Fail: Expected (%.4f, %.4f, %.4f), Got (%.4f, %.4f, "
		    "%.4f), Diff: %.6f\n",
		    b[0], b[1], b[2], a[0], a[1], a[2], len);
		return 0;
	}
	return 1;
}

int main()
{
	printf("Running SH Precision Test...\n");

	SH9 sh;
	sh_zero(&sh);

	/* Test Case 1: Directional Light along Z axis */
	vec3 light_dir = {0.0f, 0.0f, 1.0f};
	vec3 light_color = {1.0f, 1.0f, 1.0f}; /* White light */

	sh_project_directional(light_dir, light_color, &sh);

	vec3 normal = {0.0f, 0.0f, 1.0f};
	vec3 result;
	sh_eval_irradiance(normal, &sh, result);

	printf("Projected Light: Dir(0,0,1), Color(1,1,1)\n");
	printf("Evaluated Irradiance at Normal(0,0,1): (%.4f, %.4f, %.4f)\n",
	       result[0], result[1], result[2]);

	/* Note: Theoretical peak irradiance for 2nd Order SH approximation of a
	 * delta cosine lobe */
	/* is approx 1.0625 times the intensity (6.25% overshoot). */
	/* We verify against this analytical result to ensure the SH math is
	 * correct. */
	vec3 expected_sh_peak = {1.0625f, 1.0625f, 1.0625f};

	if (vec3_approx_eq(result, expected_sh_peak,
	                   0.01f)) { /* 1% tolerance relative to SH theory */
		printf("Test Passed (Matches SH Theory 1.0625).\n");
	} else {
		printf("Test Failed (Does not match SH Theory).\n");
		return 1;
	}

	/* Test Case 2: 45 degrees */
	vec3 light_dir_45 = {0.0f, 0.70710678f, 0.70710678f};
	sh_zero(&sh);
	sh_project_directional(light_dir_45, light_color, &sh);
	sh_eval_irradiance(light_dir_45, &sh, result);

	printf(
	    "Evaluated Irradiance at Normal(0, 0.707, 0.707): (%.4f, %.4f, "
	    "%.4f)\n",
	    result[0], result[1], result[2]);

	if (vec3_approx_eq(result, expected_sh_peak, 0.01f)) {
		printf("Test 45 Passed (Matches SH Theory).\n");
	} else {
		printf("Test 45 Failed.\n");
		return 1;
	}

	return 0;
}
