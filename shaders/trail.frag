#version 430 core

/*
 * trail.frag — HDR neon ribbon trail fragment shader.
 *
 * Visual quality features:
 * 1. Neon tube profile: white-hot narrow core + saturated color halo.
 * 2. Multi-layer glow: tight core, medium inner glow, soft outer halo.
 * 3. HDR emissive output — bloom post-process creates the wide neon glow.
 * 4. Age-based opacity fade along the trail (vU).
 */

in float vU;    /* Along trail: 0=head, 1=tail */
in float vV;    /* Across ribbon: 0=left edge, 1=right edge */
in vec3 vColor; /* HDR emissive color (pre-attenuated by age on CPU) */

/* Runtime-adjustable neon profile parameter */
uniform float u_core_exp; /* Core tightness (default 12.0) */

layout(location = 0) out vec4 FragColor;

void main()
{
	/* Map vV from [0,1] to a centered coordinate: 0 at center, 1 at edges
	 */
	float center = abs(vV * 2.0 - 1.0);

	/* --- Neon tube glow profile ---
	 * Three layers simulate the light distribution of a real neon tube:
	 * - Core: very tight, intense center filament (u_core_exp)
	 * - Inner glow: medium spread colored light (core_exp / 4)
	 * - Outer halo: soft diffuse edge for bloom to amplify */
	float core = exp(-center * center * u_core_exp);
	float inner = exp(-center * center * (u_core_exp * 0.25));
	float outer = exp(-center * center * 0.8);
	float glow = core * 0.45 + inner * 0.35 + outer * 0.20;

	/* --- Soft edge anti-aliasing --- */
	float edge = 1.0 - smoothstep(0.75, 1.0, center);

	/* --- White-hot center desaturation ---
	 * Real neon tubes appear near-white at the brightest point,
	 * with saturated color visible in the surrounding glow.
	 * Mix towards luminance-preserving white at the core. */
	float peak = max(vColor.r, max(vColor.g, vColor.b));
	vec3 hot_white = vec3(peak);
	vec3 neon_color = mix(vColor, hot_white, core * 0.7);

	/* Intensity boost at core — drives bloom hard for wide neon halo */
	neon_color *= (1.0 + core * 2.0);

	/* --- Age-based fade ---
	 * Quadratic fade: newer parts fully opaque, tail fades smoothly.
	 * The CPU already attenuates color intensity; this adds alpha. */
	float age_alpha = (1.0 - vU) * (1.0 - vU);

	/* --- Final composite ---
	 * High HDR values at center trigger strong bloom response.
	 * Alpha controls additive blend contribution. */
	float alpha = edge * glow * age_alpha;

	/* Discard near-invisible fragments to avoid depth/blend artifacts */
	if (alpha < 0.005) {
		discard;
	}

	FragColor = vec4(neon_color * glow, alpha);
}
