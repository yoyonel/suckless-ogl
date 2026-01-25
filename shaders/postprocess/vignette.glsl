/* ============================================================================
   EFFECT: VIGNETTE (Rounded Rect / Polynomial Falloff)
   ============================================================================
 */

/*
 * Implementation Reference:
 * Modified version of standard distance field vignette.
 * Uses a rounded box distance function to allow shape control (Roundness)
 * and a smoothstep for falloff (Smoothness).
 */

/*
 * Calculates the distance to the edge of a rounded rectangle.
 * p: Point (U,V centered at 0)
 * b: Half-dimensions of the box (1.0 for full screen)
 * r: Radius (Roundness)
 */
float sdRoundedBox(vec2 p, vec2 b, float r)
{
	vec2 q = abs(p) - b + r;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

vec3 applyVignette(vec3 color, vec2 uv)
{
	if (!enableVignette)
		return color;

	/* Center UVs: (-1 to 1) */
	vec2 uv_centered = uv * 2.0 - 1.0;

	/* Correct Aspect Ratio for distance calculation to keep roundness
	 * circular */
	/* Note: We actually often WANT the vignette to follow screen aspect
	   ratio, so we might skip aspect correction or make it optional.
	   Standard "cinematic" vignettes usually follow the frame aspect.
	   Here we don't correct aspect, so it stretches with the screen (Oval
	   on 16:9). To make it perfectly circular on any screen, we would
	   multiply x by aspect.
	*/

	/*
	 * Roundness Logic:
	 * roundness = 0.0 -> Rectangular (hard corners if not handled, but here
	 * we treat it closer to box) roundness = 1.0 -> Circular (Natural lens)
	 * We map our parameter to be intuitive.
	 */

	/*
	 * Simple Distance-Power method (Common in games)
	 * dist = distance(uv, center)
	 * vignette = smoothstep(radius, radius - softness, dist)
	 */

	/*
	 * Advanced "Rounded Box" method allow switching between Circle and Rect
	 * We effectively vary the exponent of a superellipse or just use SDF.
	 */

	/* Let's use a modified superellipse approximation which is
	 * cheaper/smoother */
	/* d = length(pow(abs(uv), vec2(roundness_exp))) */

	/* Mapping:
	   Roundness 1.0 -> distance is length(uv) (Circle)
	   Roundness 0.0 -> distance is max(abs(x), abs(y)) (Square)
	*/

	/* Safe mix implementation */
	float dist_circle = length(uv_centered);
	float dist_rect = max(abs(uv_centered.x), abs(uv_centered.y));

	/* Blend distance metrics based on roundness */
	/* Roundness 1.0 = Circle, 0.0 = Rect */
	float dist = mix(dist_rect, dist_circle, v_roundness);

	/*
	 * Falloff calculation
	 * smoothstep(start, end, x)
	 * we want 1.0 at center, 0.0 at edges.
	 * smoothness controls the 'ramp' width.
	 */

	/*
	 * Intensity controls opacit/mixing
	 * Smoothness maps to the falloff curve.
	 */

	/* Calculate falloff */
	/* We want the vignette to start affecting the image from the
	 * corners/edges inwards */
	/* Outer edge is at dist = 1.0 (approx, depends on aspect/roundness) */

	float softness = clamp(v_smoothness, 0.05, 1.0);

	/*
	 * Invert dist so 1 is center, 0 is edge? No, standard vignette is dark
	 * at edges. Factor = how much logic to KEEP. 1.0 = keep color, 0.0 =
	 * black. At center (dist=0), Factor should be 1.0. At edge (dist=1
	 * aprox), Factor should be low (depending on intensity).
	 */

	/* smoothstep(edge0, edge1, x): returns 0 if x < edge0, 1 if x > edge1
	 */
	/* We want: 1.0 when dist is small, 0.0 when dist is large. */
	/* So: 1.0 - smoothstep(start_fade, end_fade, dist) */

	/*
	 * We define the vignette as fading out starting from some radius.
	 * Let's say we always fade out to the corner.
	 * Corner dist is sqrt(2) ~ 1.414 for circle, 1.0 for rect.
	 */

	float factor =
	    1.0 - smoothstep(1.0 - softness, 1.0 + softness * 0.5, dist);

	/* Apply Intensity: Mix between 1.0 (no vignette) and Factor */
	/* Actually intensity usually means "how dark is the darkness" */

	float final_mask = mix(1.0, factor, v_intensity);

	return color * final_mask;
}
