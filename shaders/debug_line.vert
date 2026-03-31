#version 450 core
layout(location = 0) in vec3 aPos;
// Instanced Model Matrix (locations 2,3,4,5)
layout(location = 2) in mat4 aModel;
// Instanced Albedo (location 6)
layout(location = 6) in vec3 aAlbedo;

layout(location = 0) uniform mat4 view;
layout(location = 4) uniform mat4 projection;
layout(location = 8) uniform bool u_billboardMode;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vAlbedo;

@header "projection_utils.glsl"

    void
    main()
{
	vec3 worldPos;

	if (u_billboardMode) {
		// Billboard Logic
		float maxScale = length(vec3(aModel[0]));
		float sphereRadius = maxScale;
		vec3 sphereCenter = vec3(aModel[3]);

		vec4 clipPos;
		computeBillboardSphere(aPos, sphereCenter, sphereRadius, view,
		                       projection, clipPos, worldPos);
		gl_Position = clipPos;
	} else {
		// Standard Instanced Mesh (Box)
		worldPos = vec3(aModel * vec4(aPos, 1.0));
		gl_Position = projection * view * vec4(worldPos, 1.0);
	}

	vWorldPos = worldPos;
	vAlbedo = aAlbedo;
}
