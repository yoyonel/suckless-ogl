#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 VelocityOut;

in vec3 WorldPos;
in vec3 Normal;
in vec3 Albedo;
in float Metallic;
in float Roughness;
in float AO;
in vec4 CurrentClipPos;
in vec4 PreviousClipPos;

uniform vec3 camPos;
uniform sampler2D irradianceMap;
uniform sampler2D prefilterMap;
uniform sampler2D brdfLUT;
uniform int debugMode;

// Include common PBR functions
@header "pbr_functions.glsl";

void main()
{
	vec3 N = normalize(Normal);
	vec3 V = normalize(camPos - WorldPos);

	vec3 color = (debugMode != 0)
	                 ? compute_debug(N, V, Albedo, Metallic, Roughness, AO,
	                                 debugMode)
	                 : compute_pbr(N, V, Albedo, Metallic, Roughness, AO);

	FragColor = vec4(color, 1.0);

	// Calculate Velocity
	vec2 currentPosNDC = CurrentClipPos.xy / CurrentClipPos.w;
	vec2 previousPosNDC = PreviousClipPos.xy / PreviousClipPos.w;

	// UV space velocity (NDC -> UV is * 0.5 + 0.5) implies factor 0.5
	vec2 velocity = (currentPosNDC - previousPosNDC) * 0.5;
	VelocityOut = velocity;
}
