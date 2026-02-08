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
	// we clamp to NDC edge in the direction of nx.
	// Use (nx >= 0 ? 1 : -1) instead of sign() to avoid sign(0)=0 collapse.
	float p1, p2;

	if (nz1 > -0.001)
		p1 = (nx1 >= 0.0 ? 1.0 : -1.0) * 10000.0;
	else
		p1 = projScale * (nx1 / -nz1);

	if (nz2 > -0.001)
		p2 = (nx2 >= 0.0 ? 1.0 : -1.0) * 10000.0;
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

	// Extract projection scale factors once (used in multiple branches)
	float sx = projection[0][0];
	float sy = projection[1][1];

	// Additive + multiplicative epsilon for float32 robustness at all
	// scales
	if (distSq <= r2 + max(r2 * 0.005, 1e-4)) {
		// Inside sphere: cover screen with a massive quad for
		// full-screen ray-casting
		outClipPos = vec4(quadVertexPos.xy * 2.0, 0.0, 1.0);

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
	} else if (viewPos.z > sphereRadius) {
		// Sphere entirely behind camera (nearest point z > 0): cull
		outClipPos = vec4(-2.0, -2.0, 0.0, 1.0);
		outWorldPos = sphereCenterWorld;
	} else {
		float minX, maxX, minY, maxY;
		getProjectedBounds(vec2(viewPos.x, viewPos.z), sphereRadius, sx,
		                   minX, maxX);
		getProjectedBounds(vec2(viewPos.y, viewPos.z), sphereRadius, sy,
		                   minY, maxY);

		float ndc_x = (quadVertexPos.x < 0.0) ? minX : maxX;
		float ndc_y = (quadVertexPos.y < 0.0) ? minY : maxY;

		// Conservative Depth: use nearest sphere point along view axis.
		// Derive near plane from the projection matrix to avoid
		// hardcoded magic number coupling with the CPU-side NEAR_PLANE
		// constant.
		float zNear = projection[3][2] / (projection[2][2] - 1.0);
		float nearestZ = viewPos.z + sphereRadius;
		nearestZ = min(nearestZ, -(zNear + 0.01));

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
