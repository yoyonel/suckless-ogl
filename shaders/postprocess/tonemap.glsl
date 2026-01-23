/* Paramètres Tonemapper */
uniform float tonemapSlope;
uniform float tonemapToe;
uniform float tonemapShoulder;
uniform float tonemapBlackClip;
uniform float tonemapWhiteClip;

/* ============================================================================
   EFFECT: TONEMAPPING
   ============================================================================ */

/*
 * Filmic Tonemapper (Type Unreal / Hable / ACES modifié)
 */
vec3 unrealTonemap(vec3 x) {
    float a = 2.51 * tonemapSlope;
    const float b = 0.03;
    const float c = 2.43;
    float d = 0.59 * tonemapShoulder;
    float e = 0.14 * (1.1 - tonemapToe);

    vec3 res = (x * (a * x + b)) / (x * (c * x + d) + e);

    if (tonemapBlackClip > 0.001) {
        res = max(vec3(0.0), res - tonemapBlackClip) / (1.0 - tonemapBlackClip);
    }

    if (tonemapWhiteClip > 0.001) {
        float maxVal = 1.0 - tonemapWhiteClip;
        res = min(vec3(maxVal), res) / maxVal;
    }

    return clamp(res, 0.0, 1.0);
}
