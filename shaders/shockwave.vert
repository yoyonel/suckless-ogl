#version 430 core

/*
 * shockwave.vert — Screen-aligned billboard for shockwave lensing effect.
 *
 * Each shockwave is rendered as a quad centered on the impact point,
 * always facing the camera.  The quad is scaled to the current ring
 * radius.  Outputs both local quad UV (for the ring profile) and
 * screen-space UV (for sampling the scene grab texture).
 */

layout(location = 0) in vec3 a_position; /* Unit quad [-1,1] */

uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_center;     /* World-space impact position */
uniform vec3 u_camera_pos; /* Camera world position */
uniform float u_radius;    /* Current ring radius (world units) */

out vec2 vUV;       /* [-1,1] quad coordinates for ring profile */
out vec2 vScreenUV; /* [0,1] screen coordinates for scene sampling */

void main()
{
	vUV = a_position.xy;

	/* Billboard: align quad to face camera */
	vec3 to_cam = normalize(u_camera_pos - u_center);

	/* Build camera-facing basis vectors */
	vec3 world_up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(world_up, to_cam));
	vec3 up = cross(to_cam, right);

	/* Scale and position the quad */
	vec3 world_pos = u_center + right * (a_position.x * u_radius) +
	                 up * (a_position.y * u_radius);

	gl_Position = u_proj * u_view * vec4(world_pos, 1.0);

	/* Screen UV: perspective divide → NDC [-1,1] → UV [0,1] */
	vScreenUV = gl_Position.xy / gl_Position.w * 0.5 + 0.5;
}
