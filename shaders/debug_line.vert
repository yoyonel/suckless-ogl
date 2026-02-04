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

void main()
{
	vec3 worldPos;

	if (u_billboardMode) {
		// Billboard Logic (replicating pbr_ibl_billboard.vert)
		// Extract scale/radius (assuming uniform scale)
		float maxScale = length(vec3(aModel[0]));
		float sphereRadius = maxScale;  // Consistent with pbr shader
		vec3 sphereCenter = vec3(aModel[3]);

		vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
		vec3 camUp = vec3(view[0][1], view[1][1], view[2][1]);

		// Quad size calculation from pbr shader: SphereRadius * 2.0
		// * 1.5
		float quadSize = sphereRadius * 3.0;

		worldPos = sphereCenter + camRight * aPos.x * quadSize +
		           camUp * aPos.y * quadSize;
	} else {
		// Standard Instanced Mesh (Box)
		worldPos = vec3(aModel * vec4(aPos, 1.0));
	}

	vWorldPos = worldPos;
	vAlbedo = aAlbedo;
	gl_Position = projection * view * vec4(worldPos, 1.0);
}
