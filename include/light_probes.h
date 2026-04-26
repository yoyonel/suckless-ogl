#ifndef LIGHT_PROBES_H
#define LIGHT_PROBES_H

#include "sh_math.h"
#include "shader.h"
#include <cglm/cglm.h>
#include <pthread.h>

/* Forward declaration from instanced_rendering.h */
typedef struct {
	mat4 model;
	vec3 albedo;
	float metallic;
	float roughness;
	float ao;
	float padding;
} SphereInstance_POD;

/** @brief Number of 3D textures used for packing SH coefficients (7 = 28
 * channels for 9 L2 coeffs) */
enum { SH_TEXTURE_COUNT = 7 };
/** @brief Starting texture unit for SH 3D textures */
enum { TEXTURE_UNIT_SH_START = 8 };

typedef struct {
	SH9 sh_data; /* 9 coefficients, each aligned to vec4 (16 bytes) for
	                std430 */
} LightProbe;

typedef struct {
	/* Grid Data */
	LightProbe* probes; /* CPU Buffer */
	unsigned int ssbo;  /* Legacy GPU Buffer Handle (kept for debug view) */
	unsigned int sh_textures[SH_TEXTURE_COUNT]; /* 3D Textures for hardware
	                                               interpolation */

	/* Spatial Info */
	vec3 aabb_min;
	vec3 aabb_max;
	ivec3 grid_dim;
	vec3 cell_size;
	int total_probes;

	/* Scene Copy for Async Update */
	SphereInstance_POD* scene_copy;
	int scene_count;

	/* Threading */
	pthread_t worker_thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	volatile int running;
	volatile int update_pending;
	volatile int results_ready;

	/* Debug Resources */
	Shader* debug_shader;
	Shader* aabb_shader;
	unsigned int dummy_vao;
	unsigned int aabb_vao;
	unsigned int aabb_vbo;
} LightProbeGrid;

/**
 * @brief Computes the AABB for the grid based on sphere instances.
 * @param grid Pointer to grid.
 * @param spheres Array of sphere instances (generic pointer to avoid GL dep in
 * header if needed).
 * @param count Number of spheres.
 * @param stride Stride between spheres.
 * @param padding Padding to add to the AABB.
 */
void light_probe_grid_compute_aabb(LightProbeGrid* grid, const void* spheres,
                                   int count, size_t stride, float padding);

/**
 * @brief Initializes the grid structure (CPU only).
 * @param grid Pointer to grid.
 * @param dim_x Grid dimension X.
 * @param dim_y Grid dimension Y.
 * @param dim_z Grid dimension Z.
 */
void light_probe_grid_init_cpu(LightProbeGrid* grid, int dim_x, int dim_y,
                               int dim_z);

/**
 * @brief Initializes the grid structure, creates SSBO, and starts worker
 * thread.
 */
void light_probe_grid_init(LightProbeGrid* grid, int dim_x, int dim_y,
                           int dim_z);

/**
 * @brief Sets the grid AABB and computes cell sizes from dimensions.
 */
void light_probe_grid_set_bounds(LightProbeGrid* grid, vec3 aabb_min,
                                 vec3 aabb_max);

/**
 * @brief Updates the scene data used by the grid (copies it).
 */
void light_probe_grid_set_scene(LightProbeGrid* grid, const void* spheres,
                                int count, size_t stride);

/**
 * @brief Signals the worker thread to perform an SH update.
 */
void light_probe_grid_update_async(LightProbeGrid* grid);

/**
 * @brief Checks if results are ready and uploads to GPU. Should be called on
 * main thread.
 */
void light_probe_grid_sync(LightProbeGrid* grid);

/**
 * @brief Renders debug spheres for the probes.
 * @param grid Pointer to grid.
 * @param view View matrix.
 * @param proj Projection matrix.
 */
void light_probe_grid_render_debug(LightProbeGrid* grid, mat4 view, mat4 proj);

/**
 * @brief Cleans up all resources (CPU, GPU, Threads).
 */
void light_probe_grid_cleanup(LightProbeGrid* grid);

/**
 * @brief Cleans up CPU resources.
 */
void light_probe_grid_free_cpu(LightProbeGrid* grid);

#endif /* LIGHT_PROBES_H */
