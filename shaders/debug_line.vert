#version 330 core
layout(location = 0) in vec3 aPos;
// Instanced Model Matrix (locations 2,3,4,5)
layout(location = 2) in mat4 aModel;
// Instanced Albedo (location 6)
layout(location = 6) in vec3 aAlbedo;

uniform mat4 view;
uniform mat4 projection;
uniform bool u_billboardMode;

out vec3 vWorldPos;
out vec3 vAlbedo;

// Helper function (same as in pbr_ibl_billboard.vert)
void getProjectedBounds(vec2 axis, float radius, float projScale,
                        out float outMin, out float outMax)
{
	float d2 = dot(axis, axis);
	float r2 = radius * radius;
	if (d2 <= r2) {
		outMin = -1.0;
		outMax = 1.0;
		return;
	}
	float L = sqrt(max(0.0, d2 - r2));
	float nx1 = (axis.x * L - axis.y * radius) / d2;
	float nz1 = (axis.y * L + axis.x * radius) / d2;
	float nx2 = (axis.x * L + axis.y * radius) / d2;
	float nz2 = (axis.y * L - axis.x * radius) / d2;

	float p1, p2;
	if (nz1 > -0.001)
		p1 = sign(nx1) * 10000.0;
	else
		p1 = projScale * (nx1 / -nz1);

	if (nz2 > -0.001)
		p2 = sign(nx2) * 10000.0;
	else
		p2 = projScale * (nx2 / -nz2);

	outMin = min(p1, p2);
	outMax = max(p1, p2);
}

void main()
{
	vec3 worldPos;

	if (u_billboardMode) {
		// Billboard Logic
		float maxScale = length(vec3(aModel[0]));
		float sphereRadius = maxScale;
		vec3 sphereCenter = vec3(aModel[3]);

		vec3 viewPos = (view * vec4(sphereCenter, 1.0)).xyz;
		float distSq = dot(viewPos, viewPos);
		float r2 = sphereRadius * sphereRadius;

		vec4 clipPos;

		if (distSq <= r2 * 1.001) {
			clipPos = vec4(aPos.xy * 2.0, 0.0, 1.0);
			worldPos = sphereCenter;  // Degenerate world pos, but
			                          // clip pos covers screen
		} else if (viewPos.z > 0.0) {
			// Cull spheres behind camera
			clipPos = vec4(-2.0, -2.0, 0.0, 1.0);
			worldPos = sphereCenter;
		} else {
			float sx = projection[0][0];
			float sy = projection[1][1];
			float minX, maxX, minY, maxY;
			getProjectedBounds(vec2(viewPos.x, viewPos.z),
			                   sphereRadius, sx, minX, maxX);
			getProjectedBounds(vec2(viewPos.y, viewPos.z),
			                   sphereRadius, sy, minY, maxY);

			float ndc_x = (aPos.x < 0.0) ? minX : maxX;
			float ndc_y = (aPos.y < 0.0) ? minY : maxY;

			float clipW = -viewPos.z;
			float clipZ =
			    projection[2][2] * viewPos.z + projection[3][2];
			clipPos =
			    vec4(ndc_x * clipW, ndc_y * clipW, clipZ, clipW);

			// Reconstruct WorldPos for consistency (if needed by
			// frag)
			vec3 vertexViewPos;
			vertexViewPos.z = viewPos.z;
			vertexViewPos.x = ndc_x * (-viewPos.z) / sx;
			vertexViewPos.y = ndc_y * (-viewPos.z) / sy;
			vec3 worldOffset =
			    transpose(mat3(view)) * (vertexViewPos - viewPos);
			worldPos = sphereCenter + worldOffset;
		}
		gl_Position = clipPos;
	} else {
		// Standard Instanced Mesh (Box)
		worldPos = vec3(aModel * vec4(aPos, 1.0));
		gl_Position = projection * view * vec4(worldPos, 1.0);
	}

	vWorldPos = worldPos;
	vAlbedo = aAlbedo;
}
