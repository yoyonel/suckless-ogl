/* Paramètres Tonemapper */
struct TonemapParams {
    float slope;
    float toe;
    float shoulder;
    float blackClip;
    float whiteClip;
};
uniform TonemapParams tonemap;

/* ============================================================================
   EFFECT: TONEMAPPING
   ============================================================================ */

/*
 * Filmic Tonemapper (Type Unreal / Hable / ACES modifié)
 */
vec3 unrealTonemap(vec3 x) {
    float a = 2.51 * tonemap.slope;
    const float b = 0.03;
    const float c = 2.43;
    float d = 0.59 * tonemap.shoulder;
    float e = 0.14 * (1.1 - tonemap.toe);

    vec3 res = (x * (a * x + b)) / (x * (c * x + d) + e);

    if (tonemap.blackClip > 0.001) {
        res = max(vec3(0.0), res - tonemap.blackClip) / (1.0 - tonemap.blackClip);
    }

    if (tonemap.whiteClip > 0.001) {
        float maxVal = 1.0 - tonemap.whiteClip;
        res = min(vec3(maxVal), res) / maxVal;
    }

    return clamp(res, 0.0, 1.0);
}
