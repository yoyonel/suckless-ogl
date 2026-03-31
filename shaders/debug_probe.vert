#version 430 core

layout(location = 0) uniform mat4 view;
layout(location = 4) uniform mat4 projection;

layout(location = 8) uniform vec3 u_ProbeGridMin;
layout(location = 9) uniform vec3 u_ProbeGridMax;
layout(location = 10) uniform ivec3 u_ProbeGridDim;

layout(location = 0) out vec2 vUV;
flat layout(location = 1) out int vProbeIndex;

void main()
{
	int index = gl_InstanceID;

	int dimX = max(u_ProbeGridDim.x, 1);
	int dimY = max(u_ProbeGridDim.y, 1);
	int dimZ = max(u_ProbeGridDim.z, 1);

	int z = index / (dimX * dimY);
	int rem = index % (dimX * dimY);
	int y = rem / dimX;
	int x = rem % dimX;

	vec3 cellSize = vec3(0.0);
	if (dimX > 1)
		cellSize.x =
		    (u_ProbeGridMax.x - u_ProbeGridMin.x) / float(dimX - 1);
	if (dimY > 1)
		cellSize.y =
		    (u_ProbeGridMax.y - u_ProbeGridMin.y) / float(dimY - 1);
	if (dimZ > 1)
		cellSize.z =
		    (u_ProbeGridMax.z - u_ProbeGridMin.z) / float(dimZ - 1);

	vec3 probeCenter = u_ProbeGridMin + vec3(x, y, z) * cellSize;

	/* Quad generation from vertex ID */
	vec2 quadVertices[6] =
	    vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
	           vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));
	vUV = quadVertices[gl_VertexID];

	float debugRadius = 0.225;
	/* Extract camera right/up vectors from the view matrix */
	vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 up = vec3(view[0][1], view[1][1], view[2][1]);

	vec3 worldPos =
	    probeCenter + (right * vUV.x + up * vUV.y) * debugRadius;

	gl_Position = projection * view * vec4(worldPos, 1.0);

	vProbeIndex = index;
}
