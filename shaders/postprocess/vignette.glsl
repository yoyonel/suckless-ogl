/* Paramètres Vignette */
uniform int enableVignette;
uniform float vignetteIntensity;
uniform float vignetteExtent;

/* ============================================================================
   EFFECT: VIGNETTE
   ============================================================================ */

/* Effet Vignette */
vec3 applyVignette(vec3 color, vec2 uv) {
    vec2 centered = uv * 2.0 - 1.0;
    float dist = length(centered);
    float vignette = smoothstep(vignetteExtent, vignetteExtent - 0.4, dist);
    return color * mix(1.0, vignette, vignetteIntensity);
}
