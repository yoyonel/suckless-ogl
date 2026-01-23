/* Paramètres Grain */
uniform int enableGrain;
uniform float grainIntensity;
uniform float grainIntensityShadows;
uniform float grainIntensityMidtones;
uniform float grainIntensityHighlights;
uniform float grainShadowsMax;
uniform float grainHighlightsMin;
uniform float grainTexelSize;
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
    float shadowMask = 1.0 - smoothstep(0.0, grainShadowsMax, luma);

    /* Highlights: [highlightsMin, 1.0] */
    float highlightMask = smoothstep(grainHighlightsMin, 1.0, luma);

    /* Midtones: Fill the gap */
    float midtoneMask = 1.0 - shadowMask - highlightMask;
    midtoneMask = max(0.0, midtoneMask);

    /* Composite Multiplier */
    float lumaMult = shadowMask * grainIntensityShadows +
                     midtoneMask * grainIntensityMidtones +
                     highlightMask * grainIntensityHighlights;

    /* 3. Generate Noise with Texel Size */
    /* Scale UV by texel size - larger texelSize = coarser grain */
    vec2 grainUV = uv / grainTexelSize;

    /* Add time offset to animate */
    float noise = random(grainUV + vec2(time)) * 2.0 - 1.0;

    /* 4. Apply Grain */
    return color + noise * grainIntensity * lumaMult;
}
