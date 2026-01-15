#version 450 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;

out vec3 WorldPos;
out vec3 Normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main() {
    WorldPos = vec3(model * vec4(in_position, 1.0));
    Normal = mat3(transpose(inverse(model))) * in_normal;
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}