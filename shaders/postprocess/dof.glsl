/* Paramètres DoF */
uniform int enableDoF; /* Flag d'activation DoF */
uniform int enableDoFDebug; /* Flag de debug DoF */
struct DoFParams {
    float focalDistance;
    float focalRange;
    float bokehScale;
};
uniform DoFParams dof;

/* Texture floutée (1/2 res, 13-tap filter) */
uniform sampler2D dofBlurTexture;

/* ============================================================================
   EFFECT: DEPTH OF FIELD (OPTIMIZED KAWASE / JIMENEZ)
   ============================================================================ */

vec3 applyDoF(vec3 color, vec2 uv) {
    float depth = texture(depthTexture, uv).r;

    /* Early exit for skybox (before any calculation) */
    if (depth >= 0.99999) {
        return color;
    }

    /* Calculate Circle of Confusion (CoC) */
    float zNear = 0.1;
    float zFar = 1000.0;
    float z_ndc = 2.0 * depth - 1.0;
    float dist = (2.0 * zNear * zFar) / (zFar + zNear - z_ndc * (zFar - zNear));

    float coc = abs(dist - dof.focalDistance) / (dist + 0.0001);

    /* Apply focal range (in-focus zone) */
    float blurFactor = 0.0;
    if (dist > dof.focalDistance - dof.focalRange && dist < dof.focalDistance + dof.focalRange) {
        blurFactor = 0.0;
    } else {
        /* Smooth transition at edges of focal range */
        float edge = dof.focalRange;
        float distDiff = abs(dist - dof.focalDistance);
        if (distDiff < edge + 5.0) {
            blurFactor = (distDiff - edge) / 5.0;
        } else {
            blurFactor = 1.0;
        }
    }

    blurFactor *= clamp(coc * dof.bokehScale, 0.0, 1.0);
    blurFactor = clamp(blurFactor, 0.0, 1.0);

    /* OPTIMIZED: Mix with pre-blurred texture instead of real-time sampling loop */
    if (blurFactor > 0.01) {
        vec3 blurredColor = texture(dofBlurTexture, uv).rgb;
        color = mix(color, blurredColor, blurFactor);
    }

    /* Debug visualization */
    if (enableDoFDebug != 0) {
        vec3 debugColor = vec3(0.0);
        if (dist < dof.focalDistance && blurFactor > 0.0) debugColor = vec3(0.0, blurFactor, 0.0);
        else if (dist > dof.focalDistance && blurFactor > 0.0) debugColor = vec3(0.0, 0.0, blurFactor);
        return debugColor;
    }

    return color;
}
