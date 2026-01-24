/* Paramètres Bloom */
uniform sampler2D bloomTexture;
uniform int enableBloom;
struct BloomParams {
	float intensity;
};
uniform BloomParams bloom;

/* ============================================================================
   EFFECT: BLOOM
   ============================================================================
 */

vec3 applyBloom(vec3 color)
{
	vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
	return color + bloomColor * bloom.intensity;
}
