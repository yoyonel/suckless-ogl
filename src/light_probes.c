#include <glad/glad.h>

#include "light_probes.h"

#include "log.h"
#include "perf_timer.h"
#include "render_utils.h"
#include "shader.h"
#include "utils.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef TRACY_ENABLE
#include <tracy/TracyC.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

/* Helper to get position from POD */
static void get_sphere_pos(const SphereInstance_POD* sphere, vec3 dest)
{
	glm_vec3_copy((float*)sphere->model[3], dest);
}

/* Helper to get scale from POD (assuming uniform) */
static float get_sphere_radius(const SphereInstance_POD* sphere)
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
		const SphereInstance_POD* inst =
		    (const SphereInstance_POD*)(base + ((size_t)i * stride));
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
		free(grid->scene_copy);
	}
	pthread_mutex_destroy(&grid->mutex);
	pthread_cond_destroy(&grid->cond);
}

static void compute_probe_sh(const SphereInstance_POD* local_scene,
                             int local_count, vec3 probe_pos, SH9* sh_data)
{
	for (int sphere_idx = 0; sphere_idx < local_count; sphere_idx++) {
		const SphereInstance_POD* sphere = &local_scene[sphere_idx];
		vec3 sphere_pos;
		get_sphere_pos(sphere, sphere_pos);
		float radius = get_sphere_radius(sphere);

		vec3 delta;
		glm_vec3_sub(probe_pos, sphere_pos, delta);
		float dist_sq = glm_vec3_norm2(delta);

		/* Early-out: coincident or inside sphere */
		if (dist_sq < GI_EPSILON) {
			continue;
		}
		float dist = sqrtf(dist_sq);

		/* Too close: SH ringing */
		float min_d = GI_MIN_DIST_RADII * radius;
		if (dist < min_d) {
			continue;
		}

		/* Too far: negligible */
		float max_d = GI_MAX_DIST_RADII * radius;
		if (dist > max_d) {
			continue;
		}

		/* Form factor: r^2 / d^2 */
		float form_factor = (radius * radius) / dist_sq;

		/* Direction probe → sphere */
		vec3 dir;
		glm_vec3_sub(sphere_pos, probe_pos, dir);
		glm_vec3_scale(dir, 1.0F / dist, dir);

		/* Metals don't bounce diffuse light */
		float diffuse = (1.0F - (sphere->metallic * 0.0F)) *
		                form_factor * GI_BOUNCE_SCALE;

		vec3 radiance;
		glm_vec3_scale((float*)sphere->albedo, diffuse, radiance);

		sh_project_directional(dir, radiance, sh_data);
	}
}

static int is_probe_inside_sphere(vec3 probe_pos,
                                  const SphereInstance_POD* local_scene,
                                  int local_count)
{
	for (int sphere_idx = 0; sphere_idx < local_count; sphere_idx++) {
		const SphereInstance_POD* sphere = &local_scene[sphere_idx];
		vec3 sphere_pos;
		get_sphere_pos(sphere, sphere_pos);
		float radius = get_sphere_radius(sphere);

		vec3 delta;
		glm_vec3_sub(probe_pos, sphere_pos, delta);
		if (glm_vec3_norm2(delta) < (radius * radius)) {
			return 1;
		}
	}
	return 0;
}

static void light_probe_worker_compute_probe(
    LightProbeGrid* grid, int grid_x, int grid_y, int grid_z,
    const SphereInstance_POD* local_scene, int local_count)
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
	if (is_probe_inside_sphere(probe_pos, local_scene, local_count)) {
		/* Mark as invalid for debug
		 * shader */
		grid->probes[idx].sh_data.coeffs[0][3] = -1.0F;
		return;
	}

	compute_probe_sh(local_scene, local_count, probe_pos,
	                 &grid->probes[idx].sh_data);
}

static void* light_probe_worker(void* arg)
{
	LightProbeGrid* grid = (LightProbeGrid*)arg;

#ifdef TRACY_ENABLE
	TracyCSetThreadName("GI Probe Worker");
#endif

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
		SphereInstance_POD* local_scene = NULL;
		if (local_count > 0 && grid->scene_copy) {
			size_t data_size =
			    (size_t)local_count * sizeof(SphereInstance_POD);
			local_scene = (SphereInstance_POD*)malloc(data_size);
			if (local_scene) {
				(void)safe_memcpy(local_scene, data_size,
				                  grid->scene_copy, data_size);
			}
		}
		pthread_mutex_unlock(&grid->mutex);

		if (!local_scene || local_count <= 0) {
			free(local_scene);
			continue;
		}

#ifdef TRACY_ENABLE
		TracyCZoneN(gi_compute_ctx, "GI SH Compute", 1);
#endif
		PerfTimer worker_timer;
		perf_timer_start(&worker_timer);

		(void)safe_memset(
		    grid->probes,
		    (size_t)grid->total_probes * sizeof(LightProbe), 0,
		    (size_t)grid->total_probes * sizeof(LightProbe));

		for (int grid_z = 0; grid_z < grid->grid_dim[2]; grid_z++) {
			for (int grid_y = 0; grid_y < grid->grid_dim[1];
			     grid_y++) {
				for (int grid_x = 0; grid_x < grid->grid_dim[0];
				     grid_x++) {
					light_probe_worker_compute_probe(
					    grid, grid_x, grid_y, grid_z,
					    local_scene, local_count);
				}
			}
		}

		free(local_scene);

		double worker_ms = perf_timer_elapsed_ms(&worker_timer);
		LOG_DEBUG("perf.gi",
		          "GI SH Compute: %.2f ms (%d probes, %d spheres)",
		          worker_ms, grid->total_probes, local_count);
#ifdef TRACY_ENABLE
		{
			char buf[GI_LOG_BUF_SIZE];
			(void)safe_snprintf(buf, sizeof(buf),
			                    "%.2f ms | %d probes x %d spheres",
			                    worker_ms, grid->total_probes,
			                    local_count);
			TracyCZoneText(gi_compute_ctx, buf, strlen(buf));
			TracyCZoneEnd(gi_compute_ctx);
		}
#endif

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
		free(grid->scene_copy);
	}
	grid->scene_count = count;

	size_t size = count * sizeof(SphereInstance_POD);
	grid->scene_copy = (SphereInstance_POD*)malloc(size);
	if (grid->scene_copy) {
		const char* src = (const char*)spheres;
		for (int i = 0; i < count; i++) {
			(void)safe_memcpy(&grid->scene_copy[i],
			                  sizeof(SphereInstance_POD),
			                  src + ((size_t)i * stride),
			                  sizeof(SphereInstance_POD));
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
		const int mapping[SH_TEXTURE_COUNT][4][2] = {
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

void light_probe_render_debug(LightProbeGrid* grid, mat4 view, mat4 proj)
{
	if (!grid || !grid->ssbo) {
		return;
	}

#ifdef TRACY_ENABLE
	TracyCZoneN(debug_ctx, "GI Debug Probes Draw", 1);
#endif

	if (grid->debug_shader == NULL) {
		grid->debug_shader = shader_load("shaders/debug_probe.vert",
		                                 "shaders/debug_probe.frag");
		if (!grid->debug_shader) {
#ifdef TRACY_ENABLE
			TracyCZoneEnd(debug_ctx);
#endif
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

	glBindVertexArray(0);

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
		glBindVertexArray(0);
	}

	glUseProgram(0);

#ifdef TRACY_ENABLE
	{
		char buf[GI_SMALL_LOG_BUF_SIZE];
		(void)safe_snprintf(buf, sizeof(buf), "%d instances",
		                    grid->total_probes);
		TracyCZoneText(debug_ctx, buf, strlen(buf));
		TracyCZoneEnd(debug_ctx);
	}
#endif
}
