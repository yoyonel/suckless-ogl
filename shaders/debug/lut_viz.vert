#version 450 core

layout(location = 0) in vec3 a_position;  // Original RGB (0-1)

uniform sampler3D u_lut3d;
uniform mat4 u_mvp;
uniform float u_time;

out vec3 v_color;

void main()
{
	// Sample the LUT to get the transformed color
	vec3 transformed_color = texture(u_lut3d, a_position).rgb;

	v_color = transformed_color;

	// Position the point in the 3D debug cube
	// We map [0,1] to [-1, 1] for better centering in the viewport
	// We use the transformed color as the POSITION to see the deformation
	vec3 pos = (transformed_color * 2.0) - 1.0;

	gl_Position = u_mvp * vec4(pos, 1.0);
	gl_PointSize = 2.0;
}
