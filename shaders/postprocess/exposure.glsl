/* ============================================================================
   EFFECT: EXPOSURE
   ============================================================================
 */

uniform sampler2D autoExposureTexture; /* Texture 1x1 R32F */

/* Effet Exposition (Tone Mapping) */
vec3 applyExposure(vec3 color)
{
	/* Exposition linéaire simple */
	return color * e_exposure;
}

float getCombinedExposure()
{
	if (enableAutoExposure) {
		/* Auto-exposure REPLACES manual exposure (no multiplication) */
		return texture(autoExposureTexture, vec2(0.5)).r;
	} else if (enableExposure) {
		/* Manual exposure only when auto-exposure is disabled */
		return e_exposure;
	}
	return 1.0;
}
