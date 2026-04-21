// billboard_instance_ssbo.glsl — Direct SSBO access for sorted billboard
// instances. Eliminates the SSBO→VBO copy by reading instance data directly via
// gl_InstanceID. Must match the C SphereInstance struct layout exactly (128
// bytes per element with SIMD_ALIGNMENT=64 padding).

struct SphereInstance {
	mat4 model;
	vec3 albedo;
	float metallic;
	float roughness;
	float ao;
	float padding;
	float prev_center_x;
	float prev_center_y;
	float prev_center_z;
	float _pad[6];
};

layout(std430, binding = 2) readonly buffer BillboardInstanceSSBO
{
	SphereInstance billboard_instances[];
};
