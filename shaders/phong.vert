// ============= shaders/phong.vert =============
#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;

out vec3 fragNormal;
out vec3 fragPos;

uniform mat4 uMVP;

void main()
{
    fragPos = pos;
    fragNormal = normal;
    gl_Position = uMVP * vec4(pos, 1.0);
}

