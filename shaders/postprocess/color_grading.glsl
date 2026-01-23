/* Paramètres Color Grading (Unreal Style) */
uniform int enableColorGrading;
uniform float gradSaturation;
uniform float gradContrast;
uniform float gradGamma;
uniform float gradGain;
uniform float gradOffset;

/* Paramètres White Balance */
uniform float wbTemperature;
uniform float wbTint;

/* ============================================================================
   EFFECT: WHITE BALANCE
   ============================================================================ */

/*
 * White Balance (Approximation rapide)
 * Basé sur une conversion analytique de température Kelvin/Teinte vers RGB
 */
vec3 applyWhiteBalance(vec3 color) {
    /* Early exit if neutral (6500K, 0 tint) */
    if (abs(wbTemperature - 6500.0) < 1.0 && abs(wbTint) < 0.001) {
        return color;
    }

    /* Temperature shift (simple linear approximation) */
    float tempShift = (wbTemperature - 6500.0) / 10000.0;

    vec3 wb = vec3(1.0);

    if (tempShift < 0.0) {
        /* Cooler (more blue) */
        wb.b = 1.0 - tempShift;
    } else {
        /* Warmer (more red/yellow) */
        wb.r = 1.0 + tempShift;
        wb.g = 1.0 + tempShift * 0.5;
    }

    /* Tint (Green/Magenta shift) */
    wb.g += wbTint * 0.5;

    return color * wb;
}

/* ============================================================================
   EFFECT: COLOR GRADING
   ============================================================================ */

/*
 * Logique Color Grading Unreal Engine
 */
vec3 apply_color_grading(vec3 color) {
    /* 0. Apply White Balance First */
    color = applyWhiteBalance(color);

    /* 1. Saturation */
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, gradSaturation);

    /* 2. Contraste */
    color = (color - 0.5) * gradContrast + 0.5;
    color = max(vec3(0.0), color);

    /* 3. Gamma */
    if (gradGamma > 0.0) {
        color = pow(color, vec3(gradGamma));
    }

    /* 4. Gain */
    color = color * gradGain;

    /* 5. Offset */
    color = color + gradOffset;

    return max(vec3(0.0), color);
}
