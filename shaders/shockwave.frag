#version 430 core

/*
 * shockwave.frag — Procedural expanding energy ring.
 *
 * Draws a soft ring that expands and fades.  HDR emissive output
 * interacts with the bloom post-process for a natural energy glow.
 *
 * The ring profile has:
 *   - Sharp inner edge  (steep smoothstep)
 *   - Softer outer edge (wider smoothstep)
 *   - Intensity fades as the ring expands (1 - progress²)
 */

in vec2 vUV; /* [-1,1] quad coordinates */

uniform vec3 u_color;      /* HDR body color */
uniform float u_progress;  /* 0 = just spawned, 1 = expired */
uniform float u_intensity; /* 0..1 based on impact velocity */

layout(location = 0) out vec4 FragColor;

void main()
{
	float dist = length(vUV);

	/* Ring spans from progress-dependent inner to outer edge.
	 * As the ring expands (progress 0→1), the band moves outward
	 * and becomes thinner. */
	float ring_center = 0.6 + 0.3 * u_progress;
	float ring_width = 0.15 * (1.0 - 0.5 * u_progress);

	/* Soft ring profile */
	float inner = smoothstep(ring_center - ring_width, ring_center, dist);
	float outer =
	    1.0 - smoothstep(ring_center, ring_center + ring_width, dist);
	float ring = inner * outer;

	/* Fade with age: quick ramp up, slow fade out */
	float age_fade = (1.0 - u_progress * u_progress);

	/* Discard fully transparent fragments */
	float alpha = ring * age_fade * u_intensity;
	if (alpha < 0.001) {
		discard;
	}

	/* HDR emissive output — bloom will handle the glow spread */
	float hdr_scale = 4.0;
	vec3 emissive = u_color * hdr_scale * alpha;

	FragColor = vec4(emissive, alpha);
}
