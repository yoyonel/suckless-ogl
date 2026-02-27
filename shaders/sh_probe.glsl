// sh_probe.glsl

uniform vec3 u_ProbeGridMin;
uniform vec3 u_ProbeGridMax;
uniform ivec3 u_ProbeGridDim;
uniform int u_GIMode;  // 0: OFF, 1: 3D Texture, 2: SSBO

/* 7x 3D textures for SH coefficients (Units 8-14)
   - Tex 0-5: Coeffs 0-7 (RGBA16F, each holds 4 channels)
   - Tex 6: Coeff 8 (RGBA16F, only RGB used)
*/
uniform sampler3D u_SHTexture0;
uniform sampler3D u_SHTexture1;
uniform sampler3D u_SHTexture2;
uniform sampler3D u_SHTexture3;
uniform sampler3D u_SHTexture4;
uniform sampler3D u_SHTexture5;
uniform sampler3D u_SHTexture6;

struct LightProbe {
	vec4 coeffs[9];
};

layout(std430, binding = 3) readonly buffer ProbeBuffer
{
	LightProbe probes[];
};

// SH Constants (from sh_math.h)
const float Y00 = 0.28209479177387814347;   // 0.5 * sqrt(1/pi)
const float Y1n1 = 0.48860251190291992159;  // -sqrt(3/4pi)
const float Y10 = 0.48860251190291992159;
const float Y11 = 0.48860251190291992159;
const float Y2n2 = 1.09254843059207907054;  // 0.5 * sqrt(15/pi)
const float Y2n1 = 1.09254843059207907054;
const float Y20 = 0.31539156525251999825;  // 0.25 * sqrt(5/pi)
const float Y21 = 1.09254843059207907054;
const float Y22 = 0.54627421529603953527;  // 0.25 * sqrt(15/pi)

const float A0 = 3.14159265359;
const float A1 = 2.09439510239;
const float A2 = 0.78539816339;

#define USE_SH_BAKE_FACTORS

#ifdef USE_SH_BAKE_FACTORS
vec3 eval_sh_irradiance_packed(vec3 normal, vec4 t0, vec4 t1, vec4 t2, vec4 t3,
                               vec4 t4, vec4 t5, vec4 t6)
{
	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	vec3 L[9];
	/* TEX 0: {0,0}, {0,1}, {0,2}, {1,0} */
	L[0] = t0.rgb;
	L[1].r = t0.a;
	/* TEX 1: {1,1}, {1,2}, {2,0}, {2,1} */
	L[1].gb = t1.rg;
	L[2].rg = t1.ba;
	/* TEX 2: {2,2}, {3,0}, {3,1}, {3,2} */
	L[2].b = t2.r;
	L[3] = t2.gba;
	/* TEX 3: {4,0}, {4,1}, {4,2}, {5,0} */
	L[4] = t3.rgb;
	L[5].r = t3.a;
	/* TEX 4: {5,1}, {5,2}, {6,0}, {6,1} */
	L[5].gb = t4.rg;
	L[6].rg = t4.ba;
	/* TEX 5: {6,2}, {7,0}, {7,1}, {7,2} */
	L[6].b = t5.r;
	L[7] = t5.gba;
	/* TEX 6: {8,0}, {8,1}, {8,2}, {-1,-1} */
	L[8] = t6.rgb;

	vec3 color = L[0];  // L[0] multiplié par 1.0
	color += L[1] * y;
	color += L[2] * z;
	color += L[3] * x;
	color += L[4] * (x * y);
	color += L[5] * (y * z);
	color += L[6] * (3.0 * z * z - 1.0);
	color += L[7] * (x * z);
	color += L[8] * (x * x - y * y);

	return max(color, vec3(0.0));
}

vec3 eval_sh_irradiance_ssbo(vec3 normal, vec3 worldPos)
{
	vec3 grid_size = u_ProbeGridMax - u_ProbeGridMin;
	vec3 local_pos = worldPos - u_ProbeGridMin;
	vec3 t = local_pos / max(grid_size, vec3(0.001));

	// Calculate base probe index and interpolation weights
	vec3 float_idx = t * vec3(max(u_ProbeGridDim - ivec3(1), ivec3(1)));
	ivec3 base_idx = min(ivec3(floor(float_idx)),
	                     max(u_ProbeGridDim - ivec3(2), ivec3(0)));

	vec3 weights = float_idx - vec3(base_idx);
	weights = clamp(weights, 0.0, 1.0);

	vec4 interpolated_coeffs[9];
	for (int j = 0; j < 9; ++j) {
		interpolated_coeffs[j] = vec4(0.0);
	}

	for (int i = 0; i < 8; ++i) {
		ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
		ivec3 p = clamp(base_idx + offset, ivec3(0),
		                u_ProbeGridDim - ivec3(1));

		int idx = (p.z * u_ProbeGridDim.y * u_ProbeGridDim.x) +
		          (p.y * u_ProbeGridDim.x) + p.x;

		// mix(x, y, a) retourne x quand a=0, et y quand a=1.
		// vec3(offset) contient exactement des 0.0 ou des 1.0
		vec3 w3 = mix(1.0 - weights, weights, vec3(offset));
		float w = w3.x * w3.y * w3.z;

		for (int j = 0; j < 9; ++j) {
			interpolated_coeffs[j] += probes[idx].coeffs[j] * w;
		}
	}

	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	// Reconstruction ultra-rapide avec les coefficients "cuits"
	vec3 color =
	    interpolated_coeffs[0].rgb;  // Multiplié implicitement par 1.0
	color += interpolated_coeffs[1].rgb * y;
	color += interpolated_coeffs[2].rgb * z;
	color += interpolated_coeffs[3].rgb * x;
	color += interpolated_coeffs[4].rgb * (x * y);
	color += interpolated_coeffs[5].rgb * (y * z);
	color += interpolated_coeffs[6].rgb * (3.0 * z * z - 1.0);
	color += interpolated_coeffs[7].rgb * (x * z);
	color += interpolated_coeffs[8].rgb * (x * x - y * y);

	return max(color, vec3(0.0));
}
#else
vec3 eval_sh_irradiance_packed(vec3 normal, vec4 t0, vec4 t1, vec4 t2, vec4 t3,
                               vec4 t4, vec4 t5, vec4 t6)
{
	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	float c00 = Y00;
	float c1n1 = Y1n1 * y;
	float c10 = Y10 * z;
	float c11 = Y11 * x;
	float c2n2 = Y2n2 * x * y;
	float c2n1 = Y2n1 * y * z;
	float c20 = Y20 * (3.0 * z * z - 1.0);
	float c21 = Y21 * x * z;
	float c22 = Y22 * (x * x - y * y);

	vec3 L[9];
	/* TEX 0: {0,0}, {0,1}, {0,2}, {1,0} */
	L[0] = t0.rgb;
	L[1].r = t0.a;
	/* TEX 1: {1,1}, {1,2}, {2,0}, {2,1} */
	L[1].gb = t1.rg;
	L[2].rg = t1.ba;
	/* TEX 2: {2,2}, {3,0}, {3,1}, {3,2} */
	L[2].b = t2.r;
	L[3] = t2.gba;
	/* TEX 3: {4,0}, {4,1}, {4,2}, {5,0} */
	L[4] = t3.rgb;
	L[5].r = t3.a;
	/* TEX 4: {5,1}, {5,2}, {6,0}, {6,1} */
	L[5].gb = t4.rg;
	L[6].rg = t4.ba;
	/* TEX 5: {6,2}, {7,0}, {7,1}, {7,2} */
	L[6].b = t5.r;
	L[7] = t5.gba;
	/* TEX 6: {8,0}, {8,1}, {8,2}, {-1,-1} */
	L[8] = t6.rgb;

	vec3 color = vec3(0.0);
	color += L[0] * (A0 * c00);
	color += L[1] * (A1 * c1n1);
	color += L[2] * (A1 * c10);
	color += L[3] * (A1 * c11);
	color += L[4] * (A2 * c2n2);
	color += L[5] * (A2 * c2n1);
	color += L[6] * (A2 * c20);
	color += L[7] * (A2 * c21);
	color += L[8] * (A2 * c22);

	return max(color, vec3(0.0));
}

vec3 eval_sh_irradiance_ssbo(vec3 normal, vec3 worldPos)
{
	vec3 grid_size = u_ProbeGridMax - u_ProbeGridMin;
	vec3 local_pos = worldPos - u_ProbeGridMin;
	vec3 t = local_pos / max(grid_size, vec3(0.001));

	// Calculate base probe index and interpolation weights
	vec3 float_idx = t * vec3(max(u_ProbeGridDim - ivec3(1), ivec3(1)));
	ivec3 base_idx = min(ivec3(floor(float_idx)),
	                     max(u_ProbeGridDim - ivec3(2), ivec3(0)));
	vec3 weights = float_idx - vec3(base_idx);
	weights = clamp(weights, 0.0, 1.0);

	vec4 interpolated_coeffs[9];
	for (int j = 0; j < 9; ++j) {
		interpolated_coeffs[j] = vec4(0.0);
	}

	for (int i = 0; i < 8; ++i) {
		ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
		ivec3 p = clamp(base_idx + offset, ivec3(0),
		                u_ProbeGridDim - ivec3(1));
		int idx = (p.z * u_ProbeGridDim.y * u_ProbeGridDim.x) +
		          (p.y * u_ProbeGridDim.x) + p.x;

		// mix(x, y, a) retourne x quand a=0, et y quand a=1.
		// vec3(offset) contient exactement des 0.0 ou des 1.0
		vec3 w3 = mix(1.0 - weights, weights, vec3(offset));
		float w = w3.x * w3.y * w3.z;

		for (int j = 0; j < 9; ++j) {
			interpolated_coeffs[j] += probes[idx].coeffs[j] * w;
		}
	}

	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	float c00 = Y00;
	float c1n1 = Y1n1 * y;
	float c10 = Y10 * z;
	float c11 = Y11 * x;
	float c2n2 = Y2n2 * x * y;
	float c2n1 = Y2n1 * y * z;
	float c20 = Y20 * (3.0 * z * z - 1.0);
	float c21 = Y21 * x * z;
	float c22 = Y22 * (x * x - y * y);

	vec3 color = vec3(0.0);
	color += interpolated_coeffs[0].rgb * (A0 * c00);
	color += interpolated_coeffs[1].rgb * (A1 * c1n1);
	color += interpolated_coeffs[2].rgb * (A1 * c10);
	color += interpolated_coeffs[3].rgb * (A1 * c11);
	color += interpolated_coeffs[4].rgb * (A2 * c2n2);
	color += interpolated_coeffs[5].rgb * (A2 * c2n1);
	color += interpolated_coeffs[6].rgb * (A2 * c20);
	color += interpolated_coeffs[7].rgb * (A2 * c21);
	color += interpolated_coeffs[8].rgb * (A2 * c22);

	return max(color, vec3(0.0));
}
#endif

vec3 get_probe_irradiance(vec3 N, vec3 worldPos)
{
	if (u_GIMode == 0 || u_ProbeGridDim.x <= 0 || u_ProbeGridDim.y <= 0 ||
	    u_ProbeGridDim.z <= 0) {
		return vec3(0.0);
	}

	if (u_GIMode == 2) {
		return eval_sh_irradiance_ssbo(N, worldPos);
	}

	vec3 grid_size = u_ProbeGridMax - u_ProbeGridMin;
	vec3 local_pos = worldPos - u_ProbeGridMin;
	vec3 t = local_pos / max(grid_size, vec3(0.001));

	// Hardware trilinear interpolation via sampler3D
	// UVW coordinates clamped to [0,1]
	vec3 uvw = (t * vec3(u_ProbeGridDim - 1) + 0.5) / vec3(u_ProbeGridDim);
	uvw = clamp(uvw, 0.0, 1.0);

	vec4 t0 = texture(u_SHTexture0, uvw);
	vec4 t1 = texture(u_SHTexture1, uvw);
	vec4 t2 = texture(u_SHTexture2, uvw);
	vec4 t3 = texture(u_SHTexture3, uvw);
	vec4 t4 = texture(u_SHTexture4, uvw);
	vec4 t5 = texture(u_SHTexture5, uvw);
	vec4 t6 = texture(u_SHTexture6, uvw);

	return eval_sh_irradiance_packed(N, t0, t1, t2, t3, t4, t5, t6);
}
