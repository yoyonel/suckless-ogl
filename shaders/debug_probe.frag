#version 430 core

layout(location = 0) in vec2 vUV;
flat layout(location = 1) in int vProbeIndex;

layout(location = 0) uniform mat4 view;

layout(location = 0) out vec4 color;

/* SH Constants (matching sh_math.h) */
const float Y00 = 0.28209479177387814347;
const float Y1n1 = 0.48860251190291992159;
const float Y10 = 0.48860251190291992159;
const float Y11 = 0.48860251190291992159;
const float Y2n2 = 1.09254843059207907054;
const float Y2n1 = 1.09254843059207907054;
const float Y20 = 0.31539156525251999825;
const float Y21 = 1.09254843059207907054;
const float Y22 = 0.54627421529603953527;

const float A0 = 3.14159265359;
const float A1 = 2.09439510239;
const float A2 = 0.78539816339;

struct LightProbe {
	vec4 coeffs[9];
};

layout(std430, binding = 3) readonly buffer LightProbeBuffer
{
	LightProbe probes[];
};

void main()
{
	/* Check if probe is marked as invalid */
	if (probes[vProbeIndex].coeffs[0].a < -0.5)
		discard;

	/* Raytrace sphere on billboard */
	float distSq = dot(vUV, vUV);
	if (distSq > 1.0)
		discard;

	float z = sqrt(1.0 - distSq);
	vec3 normalView = vec3(vUV, z);

	/* Transform normal to world space */
	vec3 worldRight = vec3(view[0][0], view[1][0], view[2][0]);
	vec3 worldUp = vec3(view[0][1], view[1][1], view[2][1]);
	vec3 worldBack = vec3(view[0][2], view[1][2], view[2][2]);

	vec3 N = normalize(normalView.x * worldRight + normalView.y * worldUp +
	                   normalView.z * worldBack);

	float x = N.x;
	float y = N.y;
	float z_world = N.z;

	LightProbe probe = probes[vProbeIndex];

	/* Full SH irradiance reconstruction per fragment */
	vec3 irr = vec3(0.0);
	irr += probe.coeffs[0].rgb * (A0 * Y00);
	irr += probe.coeffs[1].rgb * (A1 * Y1n1 * y);
	irr += probe.coeffs[2].rgb * (A1 * Y10 * z_world);
	irr += probe.coeffs[3].rgb * (A1 * Y11 * x);
	irr += probe.coeffs[4].rgb * (A2 * Y2n2 * x * y);
	irr += probe.coeffs[5].rgb * (A2 * Y2n1 * y * z_world);
	irr +=
	    probe.coeffs[6].rgb * (A2 * Y20 * (3.0 * z_world * z_world - 1.0));
	irr += probe.coeffs[7].rgb * (A2 * Y21 * x * z_world);
	irr += probe.coeffs[8].rgb * (A2 * Y22 * (x * x - y * y));
	irr = max(irr, vec3(0.0));

	/* Output linear irradiance — postprocess handles tonemap+gamma */
	color = vec4(irr, 1.0);
}
