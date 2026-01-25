/* ============================================================================
   EFFECT: CHROMATIC ABERRATION
   ============================================================================
 */

/* Effet Aberration Chromatique (Optimized: single Motion Blur call) */
vec3 applyChromAbbr(vec2 uv)
{
	vec2 direction = uv - vec2(0.5);

	/* Get center pixel with motion blur (if enabled) */
	vec3 centerBlurred = getSceneSource(uv);

	/* Direct texture samples for R/B channels (skip motion blur for
	 * performance) Trade-off: Only green channel gets motion blur, but CA
	 * is subtle at edges */
	float r = texture(screenTexture, uv + direction * ca_strength).r;
	float b = texture(screenTexture, uv - direction * ca_strength).b;

	return vec3(r, centerBlurred.g, b);
}
