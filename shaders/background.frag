#version 450 core

in vec3 RayDir;
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec2 VelocityOut;

uniform sampler2D environmentMap;
uniform float blur_lod;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleEquirectangular(vec3 v)
{
    float phi = (abs(v.z) < 1e-5 && abs(v.x) < 1e-5) ? 0.0 : atan(v.z, v.x);
    vec2 uv = vec2(phi, asin(clamp(v.y, -1.0, 1.0)));
    uv *= invAtan;
    uv.x += 0.5;
    uv.y = 0.5 - uv.y; // Flip Y for correct top-down orientation
    return uv;
}

void main()
{
    vec2 uv = SampleEquirectangular(normalize(RayDir));
    vec3 envColor = textureLod(environmentMap, uv, 0.0).rgb;

    /* Sanitize NaN/Inf (Branchless-ish) */
    /* isnan is the only one needing replacement. isinf is handled by min */
    if (any(isnan(envColor))) envColor = vec3(0.0);

    /* Clamp max brightness */
    /* 200.0 is safe for bloom accumulation */
    envColor = min(envColor, vec3(200.0));
    envColor = max(envColor, vec3(0.0));

    FragColor = vec4(envColor, 1.0);
    VelocityOut = vec2(0.0); /* Skybox has no velocity */
}
