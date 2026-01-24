struct GrainParams {
    float intensity;
    float intensityShadows;
    float intensityMidtones;
    float intensityHighlights;
    float shadowsMax;
    float highlightsMin;
    float texelSize;
};
uniform GrainParams grain;
uniform int enableGrain;
uniform float time;

/* ============================================================================
   EFFECT: GRAIN
   ============================================================================ */

/* Effet Grain */
vec3 applyGrain(vec3 color, vec2 uv) {
    /* 1. Calculate Luminance */
    float luma = dot(color, vec3(0.299, 0.587, 0.114));

    /* 2. Calculate Intensity based on Luminance Ranges */
    /* Shadows: [0, shadowsMax] */
    float shadowMask = 1.0 - smoothstep(0.0, grain.shadowsMax, luma);

    /* Highlights: [highlightsMin, 1.0] */
    float highlightMask = smoothstep(grain.highlightsMin, 1.0, luma);

    /* Midtones: Fill the gap */
    float midtoneMask = 1.0 - shadowMask - highlightMask;
    midtoneMask = max(0.0, midtoneMask);

    /* Composite Multiplier */
    float lumaMult = shadowMask * grain.intensityShadows +
                     midtoneMask * grain.intensityMidtones +
                     highlightMask * grain.intensityHighlights;

    /* 3. Generate Noise with Texel Size */
    /* Scale UV by texel size - larger texelSize = coarser grain */
    vec2 grainUV = uv / grain.texelSize;

    /* Add time offset to animate */
    float noise = random(grainUV + vec2(time)) * 2.0 - 1.0;

    /* 4. Apply Grain */
    return color + noise * grain.intensity * lumaMult;
}
