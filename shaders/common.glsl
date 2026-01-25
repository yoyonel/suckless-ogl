/* ============================================================================
   UTILITY FUNCTIONS
   ============================================================================
 */

/* Fonction de bruit pseudo-aléatoire */
float random(vec2 co)
{
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

/* Interleaved Gradient Noise for Dithering */
float InterleavedGradientNoise(vec2 position)
{
	vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return fract(magic.z * fract(dot(position, magic.xy)));
}

/* Depth Linearization Helper */
float linearizeDepth(float d)
{
	float zNear = 0.1;
	float zFar = 1000.0;
	float z_ndc = 2.0 * d - 1.0;
	return (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear));
}
