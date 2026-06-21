#version 450 core

// OPTIMISATION 1 : Réduction du groupe de travail.
// 16x16 = 256 threads. C'est le "sweet spot" pour l'occupation des
// warps/wavefronts sur AMD/NVIDIA pour le compute image 2D, contre 1024 (32x32)
// qui sature les registres locaux. à régler/voir avec
// `pbr.c::COMPUTE_GROUP_SIZE_PBR` layout(local_size_x = 16, local_size_y = 16,
// local_size_z = 1) in;
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D envMap;
layout(binding = 1, rgba16f) restrict writeonly uniform image2D irradianceMap;

layout(location = 0) uniform float clamp_threshold;
layout(location = 1) uniform int u_offset_y;
layout(location = 2) uniform int u_max_y;

// Constantes pré-calculées
const float PI = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const float INV_PI = 0.31830988618;
const float INV_TWO_PI = 0.15915494309;

// OPTIMISATION 2 : Suppression du branchement et multiplications précalculées
vec2 fastDirToUV(vec3 v)
{
	// On retire le "abs(v.z) < 1e-5". L'implémentation matérielle de atan()
	// gère très bien les valeurs proches de 0.
	vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
	// Utilisation des inverses (multiplication = 1 cycle d'horloge,
	// division = plus lent)
	uv = uv * vec2(INV_TWO_PI, INV_PI) + 0.5;
	return uv;
}

vec3 uvToDir(vec2 uv)
{
	float phi = (uv.x - 0.5) * TWO_PI;
	float theta = (uv.y - 0.5) * PI;
	return vec3(cos(theta) * cos(phi), sin(theta), cos(theta) * sin(phi));
}

void OrthonormalBasis(vec3 n, out vec3 t, out vec3 b)
{
	vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
	t = normalize(cross(up, n));
	b = cross(n, t);
}

// OPTIMISATION 3 : Écrêtage mathématique "Branchless" (sans "if")
// La divergence dans la boucle la plus profonde tue les performances du GPU.
vec3 soft_clamp_branchless(vec3 color)
{
	float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
	float t_start = clamp_threshold;
	float t_end = clamp_threshold * 1.5;

	// Calcul du blend lisse
	float t = clamp((lum - t_start) / (t_end - t_start), 0.0, 1.0);
	float blend = 1.0 - smoothstep(0.0, 1.0, t) * 0.5;

	// Multiplicateur si dépassement brut
	float hard_limit_factor = t_end / max(lum, 1e-5);

	// Logique sans branchement via la fonction step() (instruction
	// matérielle rapide)
	float is_over = step(t_end, lum);
	float is_mid = step(t_start, lum) - is_over;
	float is_under = 1.0 - step(t_start, lum);

	float final_factor =
	    is_under * 1.0 + is_mid * blend + is_over * hard_limit_factor;

	return color * final_factor;
}

void main(void)
{
	ivec2 outSize = imageSize(irradianceMap);
	ivec2 pos = ivec2(gl_GlobalInvocationID.x,
	                  gl_GlobalInvocationID.y + u_offset_y);

	if (pos.x >= outSize.x || pos.y >= outSize.y || pos.y >= u_max_y)
		return;

	vec2 uv = vec2(pos) / vec2(outSize);
	vec3 N = normalize(uvToDir(uv));

	vec3 irradiance = vec3(0.0);
	vec3 up, right;
	OrthonormalBasis(N, right, up);

	float sampleDelta = 0.025;

	// OPTIMISATION 4 : Boucles entières pour la stabilité numérique.
	// Évite les erreurs d'arrondis des flottants dans les conditions
	// d'arrêt.
	int nTheta = int(ceil(0.5 * PI / sampleDelta));
	int nPhi = int(ceil(TWO_PI / sampleDelta));

	// OPTIMISATION 5 : Inversion des boucles et mise en cache !
	// Au lieu de calculer sin() et cos() de theta à chaque tour de phi,
	// on place theta en boucle externe.
	for (int t = 0; t < nTheta; t++) {
		float theta = float(t) * sampleDelta;

		// Calculé une seule fois par "anneau" (divise par N l'appel aux
		// fonctions trigo)
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		float weight =
		    sinTheta * cosTheta;  // Le cos(theta)*sin(theta) original

		for (int p = 0; p < nPhi; p++) {
			float phi = float(p) * sampleDelta;

			float sinPhi = sin(phi);
			float cosPhi = cos(phi);

			vec3 tangentSample = vec3(sinTheta * cosPhi,
			                          sinTheta * sinPhi, cosTheta);

			vec3 sampleVec = tangentSample.x * right +
			                 tangentSample.y * up +
			                 tangentSample.z * N;

			vec3 env_color =
			    textureLod(envMap, fastDirToUV(sampleVec), 0.0).rgb;

			/* Sanitize Input */
			if (any(isnan(env_color)) || any(isinf(env_color)))
				env_color = vec3(0.0);
			env_color =
			    max(env_color, vec3(0.0)); /* No negative light */

			env_color = soft_clamp_branchless(env_color);

			// On applique le poids pondéré directement
			irradiance += env_color * weight;
		}
	}

	float nrSamples = float(nTheta * nPhi);
	irradiance = PI * irradiance * (1.0 / nrSamples);

	// Sanitize Output de sécurité
	if (any(isnan(irradiance)) || any(isinf(irradiance)))
		irradiance = vec3(0.0);
	irradiance = max(irradiance, vec3(0.0));

	imageStore(irradianceMap, pos, vec4(irradiance, 1.0));
}
