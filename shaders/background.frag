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

void main()
{
    vec2 uv = SampleEquirectangular(normalize(RayDir));
	vec3 envColor = textureLod(environmentMap, uv, blur_lod).rgb;

	FragColor = vec4(envColor, 1.0);
}
