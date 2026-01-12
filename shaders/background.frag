#version 450 core

in vec3 RayDir;
out vec4 FragColor;

uniform samplerCube environmentMap;
uniform float blur_lod;

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
    vec3 envColor = textureLod(environmentMap, normalize(RayDir), blur_lod).rgb;

    envColor = ACESFilm(envColor);
    envColor = pow(envColor, vec3(1.0 / 2.2));

    FragColor = vec4(envColor, 1.0);
}
