#version 430 core

/*
 * shockwave.frag — Billboard lensing with chromatic aberration.
 *
 * Samples the scene grab texture behind the billboard quad and applies:
 *   1. Radial UV displacement (expanding ring pushes pixels outward)
 *   2. Per-channel chromatic aberration (R/G/B at different offsets)
 *   3. Additive HDR glow on the ring edge (drives bloom)
 *
 * The ring profile uses a Gaussian centered on the expanding wavefront.
 */

in vec2 vUV;       /* [-1,1] quad coordinates */
in vec2 vScreenUV; /* [0,1] screen-space UV for grab texture */

uniform sampler2D u_grab_tex; /* Scene color copy (pre-shockwave) */
uniform vec3 u_color;         /* HDR body color */
uniform float u_progress;     /* 0 = just spawned, 1 = expired */
uniform float u_intensity;    /* 0..1 based on impact velocity */

layout(location = 0) out vec4 FragColor;

/* Distortion strength: maximum UV displacement at peak. */
const float DISTORT_STRENGTH = 0.06;

/* Ring sharpness: controls Gaussian width (higher = thinner ring). */
const float RING_SHARPNESS = 8.0;

/* Chromatic aberration spread per channel (R, G, B multipliers). */
const float CA_SPREAD_R = 1.0;
const float CA_SPREAD_G = 1.25;
const float CA_SPREAD_B = 1.5;

/* HDR emissive scale for the additive glow on the ring edge. */
const float HDR_GLOW_SCALE = 3.0;

/* Glow contribution: how much additive glow vs pure distortion. */
const float GLOW_MIX = 0.25;

void main()
{
	/* Distance from quad center in [-1,1] space */
	float dist = length(vUV);

	/* Discard corners outside the unit circle */
	if (dist > 1.0) {
		discard;
	}

	/* Expanding ring wavefront: normalized radius moves 0→1 */
	float ring_radius = 0.3 + 0.6 * u_progress;

	/* Gaussian ring profile: peaks where dist == ring_radius */
	float delta = dist - ring_radius;
	float ring = exp(-RING_SHARPNESS * delta * delta);

	/* Temporal envelope: sin(pi*t) gives smooth fade-in/out */
	float envelope = sin(u_progress * 3.14159);

	/* Distortion factor: strong on ring, fades with time */
	float factor = DISTORT_STRENGTH * u_intensity * ring * envelope;

	/* Radial direction: push pixels outward from billboard center.
	 * In screen UV space, this creates the lens effect. */
	vec2 radial = (dist > 0.001) ? normalize(vUV) : vec2(0.0);
	vec2 offset = factor * radial;

	/* Chromatic aberration: sample R/G/B at different displacements.
	 * Scale the offset in screen UV space. */
	float red = texture(u_grab_tex, vScreenUV + offset * CA_SPREAD_R).r;
	float green = texture(u_grab_tex, vScreenUV + offset * CA_SPREAD_G).g;
	float blue = texture(u_grab_tex, vScreenUV + offset * CA_SPREAD_B).b;

	vec3 distorted = vec3(red, green, blue);

	/* Additive HDR glow on the ring edge (drives bloom) */
	float glow_alpha = ring * envelope * u_intensity;
	vec3 glow = u_color * HDR_GLOW_SCALE * glow_alpha;

	/* Mix: distorted scene + additive glow */
	vec3 result = distorted + glow * GLOW_MIX;

	/* Alpha: 1 on the ring (full replace), fading to 0 at edges.
	 * Outside the ring, we still show the undistorted scene. */
	float alpha = max(ring * envelope * u_intensity, 0.01);
	if (alpha < 0.005) {
		discard;
	}

	/* Blend: the ring region replaces the scene (via alpha=1),
	 * outer areas pass through the scene (via low alpha).
	 * Use SRC_ALPHA/ONE_MINUS_SRC_ALPHA blending. */
	FragColor = vec4(result, clamp(alpha, 0.0, 1.0));
}
