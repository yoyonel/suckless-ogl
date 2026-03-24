/* ============================================================================
   EFFECT: GRAIN (Photographic Film Emulation)
   ============================================================================
 */

/* Hash-based noise with better spatial distribution than sin(dot(...)).
 * Three decorrelated channels for per-channel film grain. */
vec3 filmHash(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
	p3 += dot(p3, p3.yxz + 33.33);
	return fract((p3.xxy + p3.yxx) * p3.zyx);
}

vec3 applyGrain(vec3 color, vec2 uv)
{
	/* 1. Luminance for zone weighting */
	float luma = dot(color, vec3(0.299, 0.587, 0.114));

	/* 2. Zone-based intensity (Shadows / Midtones / Highlights) */
	float shadowMask = 1.0 - smoothstep(0.0, g_shadowsMax, luma);
	float highlightMask = smoothstep(g_highlightsMin, 1.0, luma);
	float midtoneMask = max(0.0, 1.0 - shadowMask - highlightMask);

	float lumaMult = shadowMask * g_intensityShadows +
	                 midtoneMask * g_intensityMidtones +
	                 highlightMask * g_intensityHighlights;

	/* 3. Film-like noise: decorrelated per-channel with texel clustering
	 * We use a temporal jitter to ensure the grain pattern changes
	 * completely every frame, avoiding "scrolling streaks".
	 */
	vec2 grainUV = gl_FragCoord.xy / g_texelSize;
	vec2 jitter = filmHash(vec2(time, time * 0.618)).xy;
	vec3 noise3 = filmHash(grainUV + jitter * 10.0);
	noise3 = noise3 * 2.0 - 1.0; /* Remap to [-1, 1] */

	/* 4. Split: 70% luminance grain + 30% chromatic grain
	 * Film grain is mostly luminance variation with subtle color shifts */
	float lumaGrain = dot(noise3, vec3(0.333));
	vec3 grain = mix(vec3(lumaGrain), noise3, 0.3);

	/* 5. Overlay blend: darkens darks and brightens brights (film-like) */
	vec3 overlay = mix(
	    2.0 * color * (0.5 + grain * g_intensity * lumaMult),
	    1.0 - 2.0 * (1.0 - color) * (0.5 - grain * g_intensity * lumaMult),
	    step(0.5, color));

	return mix(color, overlay, 0.7);
}
