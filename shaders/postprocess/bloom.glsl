uniform sampler2D bloomTexture;

/* ============================================================================
   EFFECT: BLOOM
   ============================================================================
 */

vec3 applyBloom(vec3 color)
{
	vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
	return color + bloomColor * b_intensity;
}
