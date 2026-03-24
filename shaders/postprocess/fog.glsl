/* ============================================================================
   EFFECT: ATMOSPHERIC FOG (Exponential Depth-Based)
   ============================================================================
 */

/*
 * Exponential fog with near-distance offset.
 * Applied in HDR space before tonemapping so fog color participates
 * naturally in bloom and color grading.
 */
vec3 applyFog(vec3 color, vec2 uv)
{
	float depth = texture(depthTexture, uv).r;

	/* Skip skybox (depth == 1.0) — skybox already has atmospheric color */
	if (depth >= 0.9999) {
		return color;
	}

	float linearDepth = linearizeDepth(depth);

	/* Distance-based exponential fog with near offset */
	float fogDistance = max(0.0, linearDepth - fog_start);
	float fogFactor = 1.0 - exp(-fog_density * fogDistance);

	/* Height falloff attenuation (reduces fog above camera) */
	if (fog_heightFalloff > 0.001) {
		fogFactor *= exp(-fog_heightFalloff * max(0.0, fogDistance));
	}

	fogFactor = clamp(fogFactor, 0.0, 1.0);

	return mix(color, fog_color, fogFactor);
}
