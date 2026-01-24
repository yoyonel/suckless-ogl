/* Paramètres Exposition */
uniform int enableExposure;
uniform int enableAutoExposure;
struct ExposureParams {
    float exposure;
};
uniform ExposureParams exposure;
uniform sampler2D autoExposureTexture; /* Texture 1x1 R32F */

/* ============================================================================
   EFFECT: EXPOSURE
   ============================================================================ */

/* Effet Exposition (Tone Mapping) */
vec3 applyExposure(vec3 color) {
    /* Exposition linéaire simple */
    return color * exposure.exposure;
}

float getCombinedExposure() {
     if (enableAutoExposure != 0) {
        /* Auto-exposure REPLACES manual exposure (no multiplication) */
        return texture(autoExposureTexture, vec2(0.5)).r;
    } else if (enableExposure != 0) {
        /* Manual exposure only when auto-exposure is disabled */
        return exposure.exposure;
    }
    return 1.0;
}
