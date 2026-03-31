// billboard_ubo.glsl — Shared UBO for billboard rendering pipeline
// Must be included BEFORE pbr_functions.glsl / sh_probe.glsl

#define HAS_BILLBOARD_UBO

layout(std140, binding = 1) uniform BillboardBlock
{
	mat4 projection;
	mat4 view;
	mat4 previousViewProj;
	vec3 camPos;
	int debugMode;
	vec2 u_screenSize;
	vec2 _bb_pad0;
	vec3 u_ProbeGridMin;
	int u_GIMode;
	vec3 u_ProbeGridMax;
	bool u_specularAAEnabled;
	ivec3 u_ProbeGridDim;
	int u_aaMode;
	vec3 u_GridToIdxScale;
	float _bb_pad1;
};
