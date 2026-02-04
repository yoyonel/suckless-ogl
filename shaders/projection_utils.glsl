// Helper function to calculate 1D projected bounds (NDC)
void getProjectedBounds(vec2 axis, float radius, float projScale,
                        out float outMin, out float outMax)
{
	float d2 = dot(axis, axis);
	float r2 = radius * radius;

	// Check if we are inside or too close, fallback to full range if needed
	if (d2 <= r2) {
		outMin = -1.0;
		outMax = 1.0;
		return;
	}

	float L = sqrt(max(0.0, d2 - r2));

	// Tangent logic to find normal of tangent lines
	// Tangent 1
	float nx1 = (axis.x * L - axis.y * radius) / d2;
	float nz1 = (axis.y * L + axis.x * radius) / d2;

	// Tangent 2
	float nx2 = (axis.x * L + axis.y * radius) / d2;
	float nz2 = (axis.y * L - axis.x * radius) / d2;

	// Project to NDC: x_ndc = (nx / -nz) * projScale
	// Division by -nz because OpenGL looks down -Z
	// HANDLE SINGULARITY: If tangent point is behind camera (nz >= 0),
	// we clamp to infinity in the direction of nx.
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

// Full billboard sphere projection logic (Inside/Behind/Projected)
void computeBillboardSphere(vec3 quadVertexPos, vec3 sphereCenterWorld,
                            float sphereRadius, mat4 view, mat4 projection,
                            out vec4 outClipPos, out vec3 outWorldPos)
{
	vec3 viewPos = (view * vec4(sphereCenterWorld, 1.0)).xyz;
	float distSq = dot(viewPos, viewPos);
	float r2 = sphereRadius * sphereRadius;

	if (distSq <= r2 * 1.005) {
		// Inside sphere: cover screen with a massive quad for
		// full-screen ray-casting
		outClipPos = vec4(quadVertexPos.xy * 2.0, 0.0, 1.0);

		float sx = projection[0][0];
		float sy = projection[1][1];

		// Reconstruct world pos on a plane in front of the camera
		// to allow rays to be cast properly using normalize(WorldPos -
		// camPos).
		vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
		vec3 camUp = vec3(view[0][1], view[1][1], view[2][1]);
		vec3 camForward = -vec3(view[0][2], view[1][2], view[2][2]);

		// Extract camera position from view matrix
		vec3 camPos = -(transpose(mat3(view)) * view[3].xyz);

		// Place a plane at unit distance in front of the camera
		// quadVertexPos is [-0.5, 0.5], so * 2.0 is [-1, 1] (NDC range)
		outWorldPos = camPos + camForward +
		              camRight * (quadVertexPos.x * 2.0 / sx) +
		              camUp * (quadVertexPos.y * 2.0 / sy);
	} else if (viewPos.z > 0.0) {
		// Behind camera: cull
		outClipPos = vec4(-2.0, -2.0, 0.0, 1.0);
		outWorldPos = sphereCenterWorld;
	} else {
		float sx = projection[0][0];
		float sy = projection[1][1];

		float minX, maxX, minY, maxY;
		getProjectedBounds(vec2(viewPos.x, viewPos.z), sphereRadius, sx,
		                   minX, maxX);
		getProjectedBounds(vec2(viewPos.y, viewPos.z), sphereRadius, sy,
		                   minY, maxY);

		float ndc_x = (quadVertexPos.x < 0.0) ? minX : maxX;
		float ndc_y = (quadVertexPos.y < 0.0) ? minY : maxY;

		// Conservative Depth
		float nearestZ = viewPos.z + sphereRadius;
		nearestZ = min(nearestZ, -0.11);

		float clipW = -nearestZ;
		float clipZ = projection[2][2] * nearestZ + projection[3][2];

		outClipPos = vec4(ndc_x * clipW, ndc_y * clipW, clipZ, clipW);

		// Reconstruct WorldPos
		vec3 vertexViewPos;
		vertexViewPos.z = nearestZ;
		vertexViewPos.x = ndc_x * (-nearestZ) / sx;
		vertexViewPos.y = ndc_y * (-nearestZ) / sy;

		vec3 worldOffset =
		    transpose(mat3(view)) * (vertexViewPos - viewPos);
		outWorldPos = sphereCenterWorld + worldOffset;
	}
}
