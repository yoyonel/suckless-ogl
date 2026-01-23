/* Paramètres Bloom */
uniform sampler2D bloomTexture;
uniform int enableBloom;
uniform float bloomIntensity;

/* ============================================================================
   EFFECT: BLOOM
   ============================================================================ */

vec3 applyBloom(vec3 color) {
    vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
    return color + bloomColor * bloomIntensity;
}
