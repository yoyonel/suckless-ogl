#include "nbody.h"

#include "utils.h"
#include <cglm/affine.h>
#include <cglm/mat4.h>
#include <cglm/vec3.h>
#include <math.h>

/* ========================================================================= */
/* Helpers                                                                   */
/* ========================================================================= */

static void add_body(NBodySim* sim, const vec3 pos, const vec3 vel, float mass,
                     float radius, const vec3 albedo, float metallic,
                     float roughness)
{
	if (sim->body_count >= NBODY_MAX_BODIES) {
		return;
	}
	NBodyParticle* body = &sim->bodies[sim->body_count];
	glm_vec3_copy((float*)pos, body->position);
	glm_vec3_copy((float*)vel, body->velocity);
	body->mass = mass;
	body->radius = radius;
	glm_vec3_copy((float*)albedo, body->albedo);
	body->metallic = metallic;
	body->roughness = roughness;
	sim->body_count++;
}

/* Per-pair Plummer softening: ε² = max(NBODY_SOFTENING_SQ, (F·(r_i+r_j))²).
 * Used by forces, energy, and initial velocities for consistency. */
static float pair_softening_sq(float radius_i, float radius_j)
{
	float sum_r = radius_i + radius_j;
	float eps = NBODY_SOFTENING_FACTOR * sum_r;
	float eps_sq = eps * eps;
	return eps_sq > NBODY_SOFTENING_SQ ? eps_sq : NBODY_SOFTENING_SQ;
}

/* Circular-orbit speed in a Plummer-softened potential:
 * v = r · sqrt(G·M / (r² + ε²)^{3/2})  */
static float softened_orbital_vel(float grav, float central_mass, float orbit_r,
                                  float star_r, float body_r)
{
	float eps_sq = pair_softening_sq(star_r, body_r);
	float r_sq = orbit_r * orbit_r;
	float d_sq = r_sq + eps_sq;
	float d_32 = d_sq * sqrtf(d_sq);
	return orbit_r * sqrtf(grav * central_mass / d_32);
}

/* ========================================================================= */
/* Preset                                                                    */
/* ========================================================================= */

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
void nbody_init_preset(NBodySim* sim)
{
	(void)safe_memset(sim, sizeof(*sim), 0, sizeof(*sim));
	sim->gravity = NBODY_DEFAULT_G;
	sim->time_scale = 1.0F;
	sim->paused = false;

	/* Body 0: Central star — gold/bronze, heavy, stationary */
	const float star_r = 1.5F;
	add_body(sim, (vec3){0.0F, 0.0F, 0.0F}, (vec3){0.0F, 0.0F, 0.0F},
	         100.0F, star_r, (vec3){1.0F, 0.76F, 0.34F}, 1.0F, 0.2F);

	/* Body 1: Tiny fast orbiter — gold, tight circular orbit in XY plane */
	{
		const float rad = 3.0F;
		const float body_r = 0.3F;
		const float vel = softened_orbital_vel(sim->gravity, 100.0F,
		                                       rad, star_r, body_r);
		add_body(sim, (vec3){rad, 0.0F, 0.0F}, (vec3){0.0F, vel, 0.0F},
		         0.3F, body_r, (vec3){1.0F, 0.84F, 0.0F}, 1.0F, 0.1F);
	}

	/* Body 2: Chrome moon — circular orbit, slightly inclined */
	{
		const float rad = 5.0F;
		const float body_r = 0.5F;
		const float vel = softened_orbital_vel(sim->gravity, 100.0F,
		                                       rad, star_r, body_r);
		add_body(sim, (vec3){0.0F, rad, 0.5F}, (vec3){-vel, 0.0F, 0.0F},
		         1.5F, body_r, (vec3){0.77F, 0.78F, 0.78F}, 1.0F,
		         0.05F);
	}

	/* Body 3: Silver moon — different orbital plane (XZ) */
	{
		const float rad = 7.0F;
		const float body_r = 0.6F;
		const float vel = softened_orbital_vel(sim->gravity, 100.0F,
		                                       rad, star_r, body_r);
		add_body(sim, (vec3){0.0F, -1.0F, rad}, (vec3){vel, 0.0F, 0.0F},
		         2.0F, body_r, (vec3){0.85F, 0.85F, 0.90F}, 1.0F,
		         0.15F);
	}

	/* Body 4: Copper planet — wide circular orbit */
	{
		const float rad = 10.0F;
		const float body_r = 0.8F;
		const float vel = softened_orbital_vel(sim->gravity, 100.0F,
		                                       rad, star_r, body_r);
		add_body(sim, (vec3){-rad, 0.0F, 1.5F},
		         (vec3){0.0F, -vel, 0.0F}, 4.0F, body_r,
		         (vec3){0.72F, 0.45F, 0.20F}, 1.0F, 0.3F);
	}

	/* Body 5: Cyan glass comet — mildly elliptical orbit */
	{
		const float rad = 8.0F;
		const float body_r = 0.4F;
		const float vel = softened_orbital_vel(sim->gravity, 100.0F,
		                                       rad, star_r, body_r) *
		                  0.85F;
		add_body(sim, (vec3){rad, 2.0F, 2.0F},
		         (vec3){0.0F, vel, -vel * 0.3F}, 0.5F, body_r,
		         (vec3){0.15F, 0.85F, 0.95F}, 0.1F, 0.05F);
	}

	/* Body 6: Magenta glass comet — mildly elliptical, different plane */
	{
		const float rad = 11.0F;
		const float body_r = 0.35F;
		const float vel = softened_orbital_vel(sim->gravity, 100.0F,
		                                       rad, star_r, body_r) *
		                  0.85F;
		add_body(sim, (vec3){-3.0F, rad, -2.0F},
		         (vec3){vel * 0.5F, 0.0F, vel * 0.6F}, 0.5F, body_r,
		         (vec3){0.90F, 0.20F, 0.75F}, 0.1F, 0.05F);
	}

	/* Zero the total momentum so the center of mass stays fixed.
	 * v_cm = Σ(m_i * v_i) / Σ(m_i), then subtract from each body. */
	vec3 total_momentum = {0.0F, 0.0F, 0.0F};
	float total_mass = 0.0F;
	for (int i = 0; i < sim->body_count; i++) {
		vec3 mom;
		glm_vec3_scale(sim->bodies[i].velocity, sim->bodies[i].mass,
		               mom);
		glm_vec3_add(total_momentum, mom, total_momentum);
		total_mass += sim->bodies[i].mass;
	}
	vec3 vel_cm;
	glm_vec3_scale(total_momentum, 1.0F / total_mass, vel_cm);
	for (int i = 0; i < sim->body_count; i++) {
		glm_vec3_sub(sim->bodies[i].velocity, vel_cm,
		             sim->bodies[i].velocity);
	}

	/* Snapshot total energy for diagnostics (tests). */
	(void)nbody_total_energy(sim);

	/* Initialize prev_position = position for first frame (no motion). */
	for (int i = 0; i < sim->body_count; i++) {
		glm_vec3_copy(sim->bodies[i].position,
		              sim->bodies[i].prev_position);
	}
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

/* ========================================================================= */
/* Energy diagnostics                                                        */
/* ========================================================================= */

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
// NOLINTBEGIN(readability-identifier-length)

/* E = Σ ½ m v² − Σ_{i<j} G m_i m_j / sqrt(r² + ε²)
 * Conserved (bounded oscillation) by the symplectic integrator. */

float nbody_total_energy(const NBodySim* sim)
{
	float kinetic = 0.0F;
	for (int i = 0; i < sim->body_count; i++) {
		vec3 v;
		glm_vec3_copy((float*)sim->bodies[i].velocity, v);
		kinetic += 0.5F * sim->bodies[i].mass * glm_vec3_dot(v, v);
	}

	float potential = 0.0F;
	for (int i = 0; i < sim->body_count; i++) {
		for (int j = i + 1; j < sim->body_count; j++) {
			vec3 diff;
			vec3 pi;
			vec3 pj;
			glm_vec3_copy((float*)sim->bodies[i].position, pi);
			glm_vec3_copy((float*)sim->bodies[j].position, pj);
			glm_vec3_sub(pj, pi, diff);
			float eps2 = pair_softening_sq(sim->bodies[i].radius,
			                               sim->bodies[j].radius);
			float dist = sqrtf(glm_vec3_dot(diff, diff) + eps2);
			potential -= sim->gravity * sim->bodies[i].mass *
			             sim->bodies[j].mass / dist;
		}
	}

	return kinetic + potential;
}

// NOLINTEND(readability-identifier-length)
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

/* ========================================================================= */
/* Gravity & Integration                                                     */
/* ========================================================================= */

// NOLINTBEGIN(readability-identifier-length)

/* O(N²) pairwise gravity with per-pair Plummer softening.
 * ε² = max(NBODY_SOFTENING_SQ, (F·(r_i+r_j))²)  */

static void compute_accelerations(const NBodySim* sim, vec3* accel)
{
	for (int i = 0; i < sim->body_count; i++) {
		glm_vec3_zero(accel[i]);
	}

	for (int i = 0; i < sim->body_count; i++) {
		for (int j = i + 1; j < sim->body_count; j++) {
			vec3 diff;
			vec3 pi;
			vec3 pj;
			glm_vec3_copy((float*)sim->bodies[i].position, pi);
			glm_vec3_copy((float*)sim->bodies[j].position, pj);
			glm_vec3_sub(pj, pi, diff);

			float eps2 = pair_softening_sq(sim->bodies[i].radius,
			                               sim->bodies[j].radius);
			float dist_sq = glm_vec3_dot(diff, diff) + eps2;
			float inv_dist = 1.0F / sqrtf(dist_sq);
			float inv_dist3 = inv_dist * inv_dist * inv_dist;
			float grav = sim->gravity * inv_dist3;

			vec3 f;
			glm_vec3_scale(diff, grav * sim->bodies[j].mass, f);
			glm_vec3_add(accel[i], f, accel[i]);

			glm_vec3_scale(diff, grav * sim->bodies[i].mass, f);
			glm_vec3_sub(accel[j], f, accel[j]);
		}
	}
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

/* Velocity Verlet (symplectic, 2nd order).
 * Energy oscillates with bounded amplitude thanks to Plummer softening. */

static void integrate_step(NBodySim* sim,
                           float dt)  // NOLINT(readability-identifier-length)
{
	vec3 accel_old[NBODY_MAX_BODIES];
	vec3 accel_new[NBODY_MAX_BODIES];

	compute_accelerations(sim, accel_old);

	/* x += v·dt + 0.5·a·dt² */
	for (int i = 0; i < sim->body_count; i++) {
		vec3 tmp;
		glm_vec3_scale(sim->bodies[i].velocity, dt, tmp);
		glm_vec3_add(sim->bodies[i].position, tmp,
		             sim->bodies[i].position);
		glm_vec3_scale(accel_old[i], 0.5F * dt * dt, tmp);
		glm_vec3_add(sim->bodies[i].position, tmp,
		             sim->bodies[i].position);
	}

	compute_accelerations(sim, accel_new);

	/* v += 0.5·(a_old + a_new)·dt */
	for (int i = 0; i < sim->body_count; i++) {
		vec3 avg;
		glm_vec3_add(accel_old[i], accel_new[i], avg);
		glm_vec3_scale(avg, 0.5F * dt, avg);
		glm_vec3_add(sim->bodies[i].velocity, avg,
		             sim->bodies[i].velocity);
	}
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
// NOLINTEND(readability-identifier-length)

/* ========================================================================= */
/* Public API                                                                */
/* ========================================================================= */

void nbody_step(NBodySim* sim, float delta_time)
{
	if (sim->paused) {
		return;
	}

	/* Snapshot current positions for per-object motion blur.
	 * prev_position will be used by nbody_write_instances() to
	 * fill SphereInstance::prev_center for the velocity buffer. */
	for (int i = 0; i < sim->body_count; i++) {
		glm_vec3_copy(sim->bodies[i].position,
		              sim->bodies[i].prev_position);
	}

	sim->accumulator += delta_time * sim->time_scale;
	if (sim->accumulator > NBODY_MAX_ACCUMULATOR) {
		sim->accumulator = NBODY_MAX_ACCUMULATOR;
	}

	while (sim->accumulator >= NBODY_FIXED_DT) {
		integrate_step(sim, NBODY_FIXED_DT);
		sim->accumulator -= NBODY_FIXED_DT;
	}
}

void nbody_write_instances(const NBodySim* sim, SphereInstance* out)
{
	for (int i = 0; i < sim->body_count; i++) {
		const NBodyParticle* body = &sim->bodies[i];
		glm_mat4_identity(out[i].model);
		glm_translate(out[i].model, (float*)body->position);
		vec3 scale = {body->radius, body->radius, body->radius};
		glm_scale(out[i].model, scale);
		glm_vec3_copy((float*)body->albedo, out[i].albedo);
		out[i].metallic = body->metallic;
		out[i].roughness = body->roughness;
		out[i].ao = 1.0F;
		glm_vec3_copy((float*)body->prev_position, out[i].prev_center);
	}
}

int nbody_get_count(const NBodySim* sim)
{
	return sim->body_count;
}
