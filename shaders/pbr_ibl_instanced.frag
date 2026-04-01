#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 VelocityOut;

layout(location = 0) in vec3 WorldPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec3 Albedo;
layout(location = 3) in float Metallic;
layout(location = 4) in float Roughness;
layout(location = 5) in float AO;
layout(location = 6) in vec4 CurrentClipPos;
layout(location = 7) in vec4 PreviousClipPos;

layout(location = 12) uniform vec3 camPos;
layout(binding = 15) uniform sampler2D irradianceMap;
layout(binding = 16) uniform sampler2D prefilterMap;
layout(binding = 17) uniform sampler2D brdfLUT;
layout(location = 13) uniform int debugMode;

// Include common PBR functions
@header "pbr_functions.glsl";

void main()
{
	vec3 N = normalize(Normal);
	vec3 V = normalize(camPos - WorldPos);

	vec3 color =
	    (debugMode != 0)
	        ? compute_debug(N, V, Albedo, Metallic, Roughness, AO,
	                        debugMode, WorldPos)
	        : compute_pbr(N, V, Albedo, Metallic, Roughness, AO, WorldPos);

	// Store Luma in Alpha for FXAA (using sqrt approx for Gamma)
	float luma = dot(sqrt(color), vec3(0.299, 0.587, 0.114));
	FragColor = vec4(color, luma);

	// Calculate Velocity
	vec2 currentPosNDC = CurrentClipPos.xy / CurrentClipPos.w;
	vec2 previousPosNDC = PreviousClipPos.xy / PreviousClipPos.w;

	// UV space velocity (NDC -> UV is * 0.5 + 0.5) implies factor 0.5
	vec2 velocity = (currentPosNDC - previousPosNDC) * 0.5;
	VelocityOut = velocity;
}
