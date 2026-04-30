#include <glad/glad.h>

#include "light_probes.h"

#include "log.h"
#include "perf_timer.h"
#include "platform/platform_utils.h"
#include "profiler.h"
#include "render_utils.h"
#include "shader.h"
#include "sphere_types.h"
#include "utils.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define USE_SH_BAKE_FACTORS 1
#define SH_COEFF_COUNT 9

/*
 * GI 1-Bounce Configuration
 *
 * Derivation of the radiance formula:
 *   For a Lambertian sphere under unit ambient illumination:
 *     E_incident = PI  (hemisphere integral of cos-weighted unit radiance)
 *     L_exitant  = albedo/PI * E_incident = albedo
 *     Solid_angle ≈ PI * r^2 / d^2        (small-angle approximation)
 *     color_for_SH = L_exitant * Solid_angle = albedo * PI * r^2 / d^2
 *   The PI in the solid angle and the 1/PI in the BRDF cancel:
 *     color_for_SH ≈ albedo * r^2 / d^2   (= the form factor)
 *
 * GI_BOUNCE_SCALE is set to 2.0 to make color bleeding clearly
 * visible against the HDR environment map's ambient irradiance.
 */
#define GI_BOUNCE_SCALE 2.0F

/* Minimum distance: just outside the sphere surface.
 * Probes inside or brushing a sphere get degenerate form factors.
 * Tight threshold ensures probes near surfaces exclude the closest
 * sphere but capture strong directional color from the neighbor. */
#define GI_MIN_DIST_RADII 1.05F

/* Maximum distance in multiples of sphere radius.
 * Beyond this, form factor < 0.0025 — negligible. */
#define GI_MAX_DIST_RADII 3.0F
#define GI_EPSILON 0.000001F
enum {
	GI_DEBUG_PROBE_VERTICES = 6,
	GI_WIRE_CUBE_VERTICES = 24,
	GI_LOG_BUF_SIZE = 128,
	GI_SMALL_LOG_BUF_SIZE = 64
};
#define GI_HALF 0.5F
#define GI_ZERO 0.0F

typedef struct {
	vec3 pos;
	float radius;
	float min_d_sq;      // Distance minimale au carré
	float max_d_sq;      // Distance maximale au carré
	float diffuse_base;  // Facteur pré-calculé
	const float* albedo;
} CachedSphere;

/* Helper to get position from POD */
static void get_sphere_pos(const SphereInstance* sphere, vec3 dest)
{
	glm_vec3_copy((float*)sphere->model[3], dest);
}

/* Helper to get scale from POD (assuming uniform) */
static float get_sphere_radius(const SphereInstance* sphere)
{
	return glm_vec3_norm((float*)sphere->model[0]);
}

void light_probe_grid_compute_aabb(LightProbeGrid* grid, const void* spheres,
                                   int count, size_t stride, float padding)
{
	if (!grid || !spheres || count <= 0) {
		return;
	}

	const char* base = (const char*)spheres;

	vec3 min_b = {FLT_MAX, FLT_MAX, FLT_MAX};
	vec3 max_b = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

	for (int i = 0; i < count; i++) {
		const SphereInstance* inst =
		    (const SphereInstance*)(base + ((size_t)i * stride));
		vec3 pos;
		get_sphere_pos(inst, pos);

		glm_vec3_minv(min_b, pos, min_b);
		glm_vec3_maxv(max_b, pos, max_b);
	}

	glm_vec3_subs(min_b, padding, grid->aabb_min);
	glm_vec3_adds(max_b, padding, grid->aabb_max);

	if (grid->grid_dim[0] > 0 && grid->grid_dim[1] > 0 &&
	    grid->grid_dim[2] > 0) {
		vec3 size;
		glm_vec3_sub(grid->aabb_max, grid->aabb_min, size);

		if (grid->grid_dim[0] > 1) {
			grid->cell_size[0] =
			    size[0] / (float)(grid->grid_dim[0] - 1);
		} else {
			grid->cell_size[0] = 0.0F;
		}

		if (grid->grid_dim[1] > 1) {
			grid->cell_size[1] =
			    size[1] / (float)(grid->grid_dim[1] - 1);
		} else {
			grid->cell_size[1] = 0.0F;
		}

		if (grid->grid_dim[2] > 1) {
			grid->cell_size[2] =
			    size[2] / (float)(grid->grid_dim[2] - 1);
		} else {
			grid->cell_size[2] = 0.0F;
		}
	}
}

void light_probe_grid_init_cpu(LightProbeGrid* grid, int dim_x, int dim_y,
                               int dim_z)
{
	if (!grid) {
		return;
	}
	grid->grid_dim[0] = dim_x;
	grid->grid_dim[1] = dim_y;
	grid->grid_dim[2] = dim_z;
	grid->total_probes = dim_x * dim_y * dim_z;

	grid->probes =
	    (LightProbe*)calloc(grid->total_probes, sizeof(LightProbe));
	grid->scene_copy = NULL;
	grid->scene_count = 0;
	grid->ssbo = 0;
	grid->debug_shader = NULL;
	grid->dummy_vao = 0;

	pthread_mutex_init(&grid->mutex, NULL);
	pthread_cond_init(&grid->cond, NULL);
	grid->running = 0;
	grid->update_pending = 0;
	grid->results_ready = 0;
}

void light_probe_grid_free_cpu(LightProbeGrid* grid)
{
	if (!grid) {
		return;
	}
	if (grid->probes) {
		free(grid->probes);
	}
	if (grid->scene_copy) {
		platform_aligned_free(grid->scene_copy);
	}
	pthread_mutex_destroy(&grid->mutex);
	pthread_cond_destroy(&grid->cond);
}

static void compute_probe_sh(const CachedSphere* cached_scene, int local_count,
                             vec3 probe_pos, SH9* sh_data)
{
	for (int sphere_idx = 0; sphere_idx < local_count; sphere_idx++) {
		const CachedSphere* sphere = &cached_scene[sphere_idx];

		vec3 delta;
		glm_vec3_sub(probe_pos, (float*)sphere->pos, delta);
		float dist_sq = glm_vec3_norm2(delta);

		/* Early-out: coincident or inside sphere */
		if (dist_sq < GI_EPSILON) {
			continue;
		}

		/* Too close: SH ringing */
		if (dist_sq < sphere->min_d_sq) {
			continue;
		}

		/* Too far: negligible */
		if (dist_sq > sphere->max_d_sq) {
			continue;
		}

		float dist = sqrtf(dist_sq);

		/* Form factor: r^2 / d^2 (r^2 is already baked in diffuse_base)
		 */
		float form_factor = 1.0F / dist_sq;

		/* Direction probe → sphere */
		vec3 dir;
		glm_vec3_scale(delta, -1.0F / dist, dir);

		/* Metals don't bounce diffuse light */
		float diffuse = sphere->diffuse_base * form_factor;

		vec3 radiance;
		glm_vec3_scale((float*)sphere->albedo, diffuse, radiance);

		sh_project_directional(dir, radiance, sh_data);
	}
}

static int is_probe_inside_sphere(vec3 probe_pos,
                                  const CachedSphere* cached_scene,
                                  int local_count)
{
	for (int sphere_idx = 0; sphere_idx < local_count; sphere_idx++) {
		const CachedSphere* sphere = &cached_scene[sphere_idx];

		vec3 delta;
		glm_vec3_sub(probe_pos, (float*)sphere->pos, delta);
		if (glm_vec3_norm2(delta) < (sphere->radius * sphere->radius)) {
			return 1;
		}
	}
	return 0;
}

#ifdef USE_SH_BAKE_FACTORS
static void light_probe_worker_compute_probe(LightProbeGrid* grid, int grid_x,
                                             int grid_y, int grid_z,
                                             const CachedSphere* cached_scene,
                                             int local_count)
{
	int idx = (grid_z * grid->grid_dim[1] * grid->grid_dim[0]) +
	          (grid_y * grid->grid_dim[0]) + grid_x;

	vec3 probe_pos;
	probe_pos[0] = grid->aabb_min[0] + ((float)grid_x * grid->cell_size[0]);
	probe_pos[1] = grid->aabb_min[1] + ((float)grid_y * grid->cell_size[1]);
	probe_pos[2] = grid->aabb_min[2] + ((float)grid_z * grid->cell_size[2]);

	if (is_probe_inside_sphere(probe_pos, cached_scene, local_count)) {
		grid->probes[idx].sh_data.coeffs[0][3] = -1.0F;
		return;
	}

	// Calcul de base des harmoniques (inchangé)
	compute_probe_sh(cached_scene, local_count, probe_pos,
	                 &grid->probes[idx].sh_data);

	/* --- NOUVEAU : Cuisson des constantes (A * Y) --- */
	static const float SH_BAKE_FACTORS[SH_COEFF_COUNT] = {
	    0.88622692545F, /* L00:  A0 * Y00 */
	    1.02332670794F, /* L1-1: A1 * Y1n1 */
	    1.02332670794F, /* L10:  A1 * Y10 */
	    1.02332670794F, /* L11:  A1 * Y11 */
	    0.85808553081F, /* L2-2: A2 * Y2n2 */
	    0.85808553081F, /* L2-1: A2 * Y2n1 */
	    0.24770795610F, /* L20:  A2 * Y20 */
	    0.85808553081F, /* L21:  A2 * Y21 */
	    0.42904276540F  /* L22:  A2 * Y22 */
	};

	// On multiplie directement les canaux RGB de chaque coefficient
	for (int i = 0; i < SH_COEFF_COUNT; i++) {
		grid->probes[idx].sh_data.coeffs[i][0] *= SH_BAKE_FACTORS[i];
		grid->probes[idx].sh_data.coeffs[i][1] *= SH_BAKE_FACTORS[i];
		grid->probes[idx].sh_data.coeffs[i][2] *= SH_BAKE_FACTORS[i];
	}
}
#else
static void light_probe_worker_compute_probe(LightProbeGrid* grid, int grid_x,
                                             int grid_y, int grid_z,
                                             const CachedSphere* cached_scene,
                                             int local_count)
{
	int idx = (grid_z * grid->grid_dim[1] * grid->grid_dim[0]) +
	          (grid_y * grid->grid_dim[0]) + grid_x;

	vec3 probe_pos;
	probe_pos[0] = grid->aabb_min[0] + ((float)grid_x * grid->cell_size[0]);
	probe_pos[1] = grid->aabb_min[1] + ((float)grid_y * grid->cell_size[1]);
	probe_pos[2] = grid->aabb_min[2] + ((float)grid_z * grid->cell_size[2]);

	/* Dual-Grid Strategy: Probes should be
	 * half-spacing away from centers. Mark
	 * as invalid/skip if inside sphere for
	 * robustness. */
	if (is_probe_inside_sphere(probe_pos, cached_scene, local_count)) {
		/* Mark as invalid for debug
		 * shader */
		grid->probes[idx].sh_data.coeffs[0][3] = -1.0F;
		return;
	}

	compute_probe_sh(cached_scene, local_count, probe_pos,
	                 &grid->probes[idx].sh_data);
}
#endif

static void* precompute_cached_spheres(SphereInstance* local_scene,
                                       int local_count)
{
	CachedSphere* cached_scene = malloc(local_count * sizeof(CachedSphere));
	for (int i = 0; i < local_count; i++) {
		get_sphere_pos(&local_scene[i], cached_scene[i].pos);
		float radius = get_sphere_radius(&local_scene[i]);
		cached_scene[i].radius = radius;

		// On précalcule les distances au carré pour éviter les
		// sqrt() plus tard
		cached_scene[i].min_d_sq =
		    (GI_MIN_DIST_RADII * radius) * (GI_MIN_DIST_RADII * radius);
		cached_scene[i].max_d_sq =
		    (GI_MAX_DIST_RADII * radius) * (GI_MAX_DIST_RADII * radius);

		// Le (sphere->metallic * 0.0F) semble être un
		// placeholder, je le garde tel quel
		cached_scene[i].diffuse_base =
		    (1.0F - (local_scene[i].metallic * 0.0F)) *
		    (radius * radius) * GI_BOUNCE_SCALE;
		cached_scene[i].albedo = local_scene[i].albedo;
	}
	return cached_scene;
}

static void* light_probe_worker(void* arg)
{
	LightProbeGrid* grid = (LightProbeGrid*)arg;

	PROFILE_THREAD_NAME("GI Probe Worker");

	while (1) {
		pthread_mutex_lock(&grid->mutex);
		while (grid->running && !grid->update_pending) {
			pthread_cond_wait(&grid->cond, &grid->mutex);
		}

		if (!grid->running) {
			pthread_mutex_unlock(&grid->mutex);
			break;
		}

		grid->update_pending = 0;

		/* Snapshot scene data under lock to avoid race with
		 * set_scene() */
		int local_count = grid->scene_count;
		SphereInstance* local_scene = NULL;
		if (local_count > 0 && grid->scene_copy) {
			size_t data_size =
			    (size_t)local_count * sizeof(SphereInstance);
			local_scene = (SphereInstance*)platform_aligned_alloc(
			    data_size, SIMD_ALIGNMENT);
			if (local_scene) {
				(void)safe_memcpy(local_scene, data_size,
				                  grid->scene_copy, data_size);
			}
		}
		pthread_mutex_unlock(&grid->mutex);

		if (!local_scene || local_count <= 0) {
			platform_aligned_free(local_scene);
			continue;
		}

		PROFILE_ZONE(gi_compute_ctx, "GI SH Compute");
		PerfTimer worker_timer;
		perf_timer_start(&worker_timer);

		(void)safe_memset(
		    grid->probes,
		    (size_t)grid->total_probes * sizeof(LightProbe), 0,
		    (size_t)grid->total_probes * sizeof(LightProbe));

		CachedSphere* cached_scene =
		    precompute_cached_spheres(local_scene, local_count);

		for (int grid_z = 0; grid_z < grid->grid_dim[2]; grid_z++) {
			for (int grid_y = 0; grid_y < grid->grid_dim[1];
			     grid_y++) {
				for (int grid_x = 0; grid_x < grid->grid_dim[0];
				     grid_x++) {
					light_probe_worker_compute_probe(
					    grid, grid_x, grid_y, grid_z,
					    cached_scene, local_count);
				}
			}
		}

		platform_aligned_free(local_scene);
		free(cached_scene);

		double worker_ms = perf_timer_elapsed_ms(&worker_timer);
		LOG_DEBUG("perf.gi",
		          "GI SH Compute: %.2f ms (%d probes, %d spheres)",
		          worker_ms, grid->total_probes, local_count);

		char buf[GI_LOG_BUF_SIZE];
		(void)safe_snprintf(buf, sizeof(buf),
		                    "%.2f ms | %d probes x %d spheres",
		                    worker_ms, grid->total_probes, local_count);
		PROFILE_ZONE_TEXT(gi_compute_ctx, buf, strlen(buf));
		PROFILE_ZONE_END(gi_compute_ctx);

		pthread_mutex_lock(&grid->mutex);
		grid->results_ready = 1;
		pthread_mutex_unlock(&grid->mutex);
	}
	return NULL;
}

void light_probe_grid_init(LightProbeGrid* grid, int dim_x, int dim_y,
                           int dim_z)
{
	light_probe_grid_init_cpu(grid, dim_x, dim_y, dim_z);

	glGenBuffers(1, &grid->ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, grid->ssbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
	             (GLsizeiptr)(grid->total_probes * sizeof(LightProbe)),
	             NULL, GL_DYNAMIC_COPY);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glGenVertexArrays(1, &grid->dummy_vao);

	/* Initialize AABB Debug Resources */
	render_utils_create_wire_cube_vbo(&grid->aabb_vbo);
	glGenVertexArrays(1, &grid->aabb_vao);
	glBindVertexArray(grid->aabb_vao);
	glBindBuffer(GL_ARRAY_BUFFER, grid->aabb_vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
	                      (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	/* Create 3D Textures for SH Coefficients */
	glGenTextures(SH_TEXTURE_COUNT, grid->sh_textures);
	for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
		glBindTexture(GL_TEXTURE_3D, grid->sh_textures[i]);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER,
		                GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER,
		                GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S,
		                GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T,
		                GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R,
		                GL_CLAMP_TO_EDGE);

		/* Allocate storage: (X, Y, Z) with RGBA16F */
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, dim_x, dim_y, dim_z,
		             0, GL_RGBA, GL_HALF_FLOAT, NULL);
	}
	glBindTexture(GL_TEXTURE_3D, 0);

	grid->running = 1;
	pthread_create(&grid->worker_thread, NULL, light_probe_worker, grid);
}

void light_probe_grid_set_bounds(LightProbeGrid* grid, vec3 aabb_min,
                                 vec3 aabb_max)
{
	if (!grid) {
		return;
	}
	glm_vec3_copy(aabb_min, grid->aabb_min);
	glm_vec3_copy(aabb_max, grid->aabb_max);

	vec3 size;
	glm_vec3_sub(grid->aabb_max, grid->aabb_min, size);

	grid->cell_size[0] = (grid->grid_dim[0] > 1)
	                         ? size[0] / (float)(grid->grid_dim[0] - 1)
	                         : GI_ZERO;
	grid->cell_size[1] = (grid->grid_dim[1] > 1)
	                         ? size[1] / (float)(grid->grid_dim[1] - 1)
	                         : GI_ZERO;
	grid->cell_size[2] = (grid->grid_dim[2] > 1)
	                         ? size[2] / (float)(grid->grid_dim[2] - 1)
	                         : GI_ZERO;
}

void light_probe_grid_set_scene(LightProbeGrid* grid, const void* spheres,
                                int count, size_t stride)
{
	if (!grid) {
		return;
	}

	pthread_mutex_lock(&grid->mutex);

	if (grid->scene_copy) {
		platform_aligned_free(grid->scene_copy);
	}
	grid->scene_count = count;

	size_t size = count * sizeof(SphereInstance);
	grid->scene_copy =
	    (SphereInstance*)platform_aligned_alloc(size, SIMD_ALIGNMENT);
	if (grid->scene_copy) {
		const char* src = (const char*)spheres;
		for (int i = 0; i < count; i++) {
			(void)safe_memcpy(
			    &grid->scene_copy[i], sizeof(SphereInstance),
			    src + ((size_t)i * stride), sizeof(SphereInstance));
		}
	}
	pthread_mutex_unlock(&grid->mutex);
}

void light_probe_grid_update_async(LightProbeGrid* grid)
{
	if (!grid) {
		return;
	}
	pthread_mutex_lock(&grid->mutex);
	if (!grid->update_pending) {
		grid->update_pending = 1;
		pthread_cond_signal(&grid->cond);
	}
	pthread_mutex_unlock(&grid->mutex);
}

void light_probe_grid_sync(LightProbeGrid* grid)
{
	if (!grid || !grid->ssbo) {
		return;
	}

	if (pthread_mutex_trylock(&grid->mutex) != 0) {
		return;
	}

	if (!grid->results_ready) {
		pthread_mutex_unlock(&grid->mutex);
		return;
	}

	/* 1. Legacy SSBO upload (for debug view) */
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, grid->ssbo);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
	                (GLsizeiptr)(grid->total_probes * sizeof(LightProbe)),
	                grid->probes);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	/* 2. 3D Texture packing and upload */
	size_t float_count = (size_t)grid->total_probes * 4;
	float* pack_buffer = malloc(float_count * sizeof(float));

	if (pack_buffer) {
		static const int mapping[SH_TEXTURE_COUNT][4][2] = {
		    /* {coeff_idx, channel_idx} */
		    {{0, 0}, {0, 1}, {0, 2}, {1, 0}},  /* Tex 0 */
		    {{1, 1}, {1, 2}, {2, 0}, {2, 1}},  /* Tex 1 */
		    {{2, 2}, {3, 0}, {3, 1}, {3, 2}},  /* Tex 2 */
		    {{4, 0}, {4, 1}, {4, 2}, {5, 0}},  /* Tex 3 */
		    {{5, 1}, {5, 2}, {6, 0}, {6, 1}},  /* Tex 4 */
		    {{6, 2}, {7, 0}, {7, 1}, {7, 2}},  /* Tex 5 */
		    {{8, 0}, {8, 1}, {8, 2}, {-1, -1}} /* Tex 6 */
		};

		for (int tex_idx = 0; tex_idx < SH_TEXTURE_COUNT; tex_idx++) {
			for (int probe_idx = 0; probe_idx < grid->total_probes;
			     probe_idx++) {
				for (int comp_idx = 0; comp_idx < 4;
				     comp_idx++) {
					int coeff_idx =
					    mapping[tex_idx][comp_idx][0];
					int channel_idx =
					    mapping[tex_idx][comp_idx][1];

					if (coeff_idx == -1) {
						pack_buffer[(probe_idx * 4) +
						            comp_idx] = 0.0F;
					} else {
						pack_buffer[(probe_idx * 4) +
						            comp_idx] =
						    grid->probes[probe_idx]
						        .sh_data
						        .coeffs[coeff_idx]
						               [channel_idx];
					}
				}
			}

			glBindTexture(GL_TEXTURE_3D,
			              grid->sh_textures[tex_idx]);
			glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0,
			                grid->grid_dim[0], grid->grid_dim[1],
			                grid->grid_dim[2], GL_RGBA, GL_FLOAT,
			                pack_buffer);
		}
		glBindTexture(GL_TEXTURE_3D, 0);
		free(pack_buffer);
	}

	glMemoryBarrier((GLbitfield)GL_SHADER_STORAGE_BARRIER_BIT |
	                (GLbitfield)GL_TEXTURE_FETCH_BARRIER_BIT);

	LOG_DEBUG("perf.gi", "GI Sync: %d probes updated (SSBO + 7x 3D Tex)",
	          grid->total_probes);

	grid->results_ready = 0;
	pthread_mutex_unlock(&grid->mutex);
}

void light_probe_grid_cleanup(LightProbeGrid* grid)
{
	if (!grid) {
		return;
	}

	pthread_mutex_lock(&grid->mutex);
	grid->running = 0;
	pthread_cond_signal(&grid->cond);
	pthread_mutex_unlock(&grid->mutex);

	if (grid->worker_thread) {
		pthread_join(grid->worker_thread, NULL);
	}

	if (grid->ssbo) {
		glDeleteBuffers(1, &grid->ssbo);
		grid->ssbo = 0;
	}

	if (grid->dummy_vao) {
		glDeleteVertexArrays(1, &grid->dummy_vao);
		grid->dummy_vao = 0;
	}

	if (grid->debug_shader) {
		shader_destroy(grid->debug_shader);
		grid->debug_shader = NULL;
	}

	if (grid->aabb_shader) {
		shader_destroy(grid->aabb_shader);
		grid->aabb_shader = NULL;
	}

	if (grid->aabb_vao) {
		glDeleteVertexArrays(1, &grid->aabb_vao);
		grid->aabb_vao = 0;
	}

	if (grid->aabb_vbo) {
		glDeleteBuffers(1, &grid->aabb_vbo);
		grid->aabb_vbo = 0;
	}

	glDeleteTextures(SH_TEXTURE_COUNT, grid->sh_textures);

	light_probe_grid_free_cpu(grid);
}

void light_probe_grid_render_debug(LightProbeGrid* grid, mat4 view, mat4 proj)
{
	if (!grid || !grid->ssbo) {
		return;
	}

	PROFILE_ZONE(debug_ctx, "GI Debug Probes Draw");

	if (grid->debug_shader == NULL) {
		grid->debug_shader = shader_load("shaders/debug_probe.vert",
		                                 "shaders/debug_probe.frag");
		if (!grid->debug_shader) {
			PROFILE_ZONE_END(debug_ctx);
			return;
		}
	}

	shader_use(grid->debug_shader);

	shader_set_mat4(grid->debug_shader, "view", (const float*)view);
	shader_set_mat4(grid->debug_shader, "projection", (const float*)proj);

	shader_set_vec3(grid->debug_shader, "u_ProbeGridMin", grid->aabb_min);
	shader_set_vec3(grid->debug_shader, "u_ProbeGridMax", grid->aabb_max);

	GLint loc_dim =
	    shader_get_uniform_location(grid->debug_shader, "u_ProbeGridDim");
	if (loc_dim >= 0) {
		glUniform3i(loc_dim, grid->grid_dim[0], grid->grid_dim[1],
		            grid->grid_dim[2]);
	}

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, grid->ssbo);

	glBindVertexArray(grid->dummy_vao);

	glDrawArraysInstanced(GL_TRIANGLES, 0, GI_DEBUG_PROBE_VERTICES,
	                      grid->total_probes);

	/* Render AABB Wireframe */
	if (grid->aabb_shader == NULL) {
		grid->aabb_shader = shader_load("shaders/debug_line.vert",
		                                "shaders/debug_line.frag");
	}

	if (grid->aabb_shader) {
		shader_use(grid->aabb_shader);
		shader_set_mat4(grid->aabb_shader, "view", (const float*)view);
		shader_set_mat4(grid->aabb_shader, "projection",
		                (const float*)proj);
		shader_set_int(grid->aabb_shader, "u_billboardMode", 0);
		shader_set_int(grid->aabb_shader, "u_stippled", 0);
		shader_set_int(grid->aabb_shader, "u_useInstanceColor", 0);
		float yellow[4] = {1.0F, 1.0F, 0.0F, 1.0F};
		shader_set_vec4(grid->aabb_shader, "u_color", yellow);

		/* Calculate Model Matrix for AABB */
		mat4 model;
		vec3 center;
		vec3 size;
		glm_vec3_add(grid->aabb_min, grid->aabb_max, center);
		glm_vec3_scale(center, GI_HALF, center);
		glm_vec3_sub(grid->aabb_max, grid->aabb_min, size);
		glm_vec3_scale(size, GI_HALF, size);

		glm_mat4_identity(model);
		glm_translate(model, center);
		glm_scale(model, size);

		/* The debug_line shader expects aModel at location 2 as an
		 * attribute. Since we're rendering a single box, we can just
		 * use glVertexAttrib to set a constant value for the model
		 * matrix. */
		for (int i = 0; i < 4; i++) {
			glVertexAttrib4fv(2 + i, (const float*)model[i]);
		}

		glBindVertexArray(grid->aabb_vao);
		glDrawArrays(GL_LINES, 0, GI_WIRE_CUBE_VERTICES);
	}

	glUseProgram(0);

	char buf[GI_SMALL_LOG_BUF_SIZE];
	(void)safe_snprintf(buf, sizeof(buf), "%d instances",
	                    grid->total_probes);
	PROFILE_ZONE_TEXT(debug_ctx, buf, strlen(buf));
	PROFILE_ZONE_END(debug_ctx);
}
