#version 430 core

/*
 * trail.frag — HDR ribbon trail fragment shader.
 *
 * Visual quality features:
 * 1. Soft anti-aliased edges via smoothstep across ribbon width (vV).
 * 2. Smooth age-based opacity fade along the trail (vU).
 * 3. HDR emissive output — bloom post-process picks up values > threshold.
 * 4. Core glow: brighter center, dimmer edges for a natural light-trail look.
 */

in float vU;    /* Along trail: 0=head, 1=tail */
in float vV;    /* Across ribbon: 0=left edge, 1=right edge */
in vec3 vColor; /* HDR emissive color (pre-attenuated by age on CPU) */

layout(location = 0) out vec4 FragColor;

void main()
{
	/* --- Soft edge anti-aliasing ---
	 * Map vV from [0,1] to a centered coordinate [-1,1],
	 * then apply a smooth falloff at the edges.
	 * This creates a soft, rounded ribbon profile. */
	float center = abs(vV * 2.0 - 1.0); /* 0 at center, 1 at edges */
	float edge_softness = 1.0 - smoothstep(0.6, 1.0, center);

	/* --- Core glow ---
	 * Brighter at the center of the ribbon, dimmer at edges.
	 * Creates a natural volumetric light appearance. */
	float core = exp(-center * center * 2.0);
	float glow = mix(0.4, 1.0, core);

	/* --- Age-based fade ---
	 * Quadratic fade: newer parts fully opaque, tail fades smoothly.
	 * The CPU already attenuates color intensity; this adds alpha. */
	float age_alpha = (1.0 - vU) * (1.0 - vU);

	/* --- Final composite ---
	 * Color is HDR (already > 1.0 from CPU), bloom catches the excess.
	 * Alpha controls additive blend contribution. */
	float alpha = edge_softness * glow * age_alpha;

	/* Discard near-invisible fragments to avoid depth/blend artifacts */
	if (alpha < 0.005) {
		discard;
	}

	FragColor = vec4(vColor * glow, alpha);
}
