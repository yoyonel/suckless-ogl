#version 430 core

/*
 * trail.vert — Camera-facing ribbon trail vertex shader.
 *
 * Input: CPU-generated triangle strip with position, UV, and HDR color.
 * The geometry is already camera-billboarded on the CPU side.
 */

/* Attribute 0: position.xyz + u (along trail, 0=head 1=tail) */
layout(location = 0) in vec4 a_position_u;

/* Attribute 1: color.rgb + v (across ribbon, 0=left 1=right) */
layout(location = 1) in vec4 a_color_v;

uniform mat4 u_view;
uniform mat4 u_proj;

out float vU;    /* Along trail: 0=head, 1=tail */
out float vV;    /* Across ribbon: 0.0 or 1.0 */
out vec3 vColor; /* HDR emissive color */

void main()
{
	vec3 worldPos = a_position_u.xyz;
	vU = a_position_u.w;
	vColor = a_color_v.rgb;
	vV = a_color_v.w;

	gl_Position = u_proj * u_view * vec4(worldPos, 1.0);
}
