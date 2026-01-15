#version 450 core

layout (location = 0) in vec3 in_position;

out vec2 LocalPos;
out float SphereRadius;
out vec3 CenterVS;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main() {
    vec3 center_world = vec3(model * vec4(0.0, 0.0, 0.0, 1.0));
    float radius = length(model[0].xyz); // uniform scale
    vec3 center_vs = vec3(view * vec4(center_world, 1.0));

    const float QUAD_SCALE = 2.0;
    vec3 pos_vs = center_vs + vec3(in_position.xy * radius * QUAD_SCALE, 0.0);

    LocalPos = in_position.xy * QUAD_SCALE;
    SphereRadius = radius;
    CenterVS = center_vs;

    gl_Position = projection * vec4(pos_vs, 1.0);
}