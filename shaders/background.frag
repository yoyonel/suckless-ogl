#version 450 core

in vec3 RayDir;
out vec4 FragColor;

uniform sampler2D environmentMap;
uniform float blur_lod;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleEquirectangular(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv.x += 0.5;
    uv.y = 0.5 - uv.y; // Flip Y for correct top-down orientation
    return uv;
}

vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) /(x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec2 uv = SampleEquirectangular(normalize(RayDir));
    vec3 envColor = textureLod(environmentMap, uv, blur_lod).rgb;

    envColor = ACESFilm(envColor);
    envColor = pow(envColor, vec3(1.0 / 2.2));

    FragColor = vec4(envColor, 1.0);
}
