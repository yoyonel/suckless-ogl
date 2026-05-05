#pragma GCC optimize("no-fast-math")
#include "nbody.h"

#include "utils.h"
#include <cglm/affine.h>
#include <cglm/mat4.h>
#include <cglm/vec3.h>
#include <math.h>

/** Small epsilon to avoid division by zero. */
static const float EPSILON = 1e-6F;

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

/* Central star properties for the default preset. */
static const float CENTRAL_STAR_MASS = 100.0F;
static const float CENTRAL_STAR_RADIUS = 1.5F;
static const vec3 CENTRAL_STAR_ALBEDO = {1.0F, 0.76F, 0.34F};
static const float CENTRAL_STAR_METALLIC = 1.0F;
static const float CENTRAL_STAR_ROUGHNESS = 0.2F;

/* Physics half-factor for kinetic energy (½mv²) and Verlet integration. */
static const float HALF = 0.5F;

/* Descriptor for an orbiting body in the preset. */
struct OrbiterDef {
	vec3 pos;      /**< Initial position. */
	vec3 vel_dir;  /**< Velocity direction (each component × orbital speed).
	                */
	float orbit_r; /**< Orbital radius (for speed computation). */
	float body_r;  /**< Body radius. */
	float mass;
	vec3 albedo;
	float metallic;
	float roughness;
	float ecc; /**< Eccentricity factor on orbital speed (1 = circular). */
};

/* clang-format off */
static const struct OrbiterDef ORBITERS[] = {
	/*          position              vel_dir        orb_r  r     mass  albedo              met   rough ecc  */
	/* 1  Gold orbiter      */ {{ 3, 0, 0},      { 0, 1, 0},     3,   0.30F, 0.3F, {1.0F,0.84F,0.0F},  1.0F, 0.10F, 1.00F},
	/* 2  Chrome moon       */ {{ 0, 5, 0.5F},   {-1, 0, 0},     5,   0.50F, 1.5F, {0.77F,0.78F,0.78F},1.0F, 0.05F, 1.00F},
	/* 3  Silver moon       */ {{ 0,-1, 7},      { 1, 0, 0},     7,   0.60F, 2.0F, {0.85F,0.85F,0.90F},1.0F, 0.15F, 1.00F},
	/* 4  Copper planet     */ {{-10, 0, 1.5F},  { 0,-1, 0},    10,   0.80F, 4.0F, {0.72F,0.45F,0.20F},1.0F, 0.30F, 1.00F},
	/* 5  Cyan comet        */ {{ 8, 2, 2},      { 0, 1,-0.3F},  8,   0.40F, 0.5F, {0.15F,0.85F,0.95F},0.1F, 0.05F, 0.85F},
	/* 6  Magenta comet     */ {{-3,11,-2},      { 0.5F, 0, 0.6F},11,  0.35F, 0.5F, {0.90F,0.20F,0.75F},0.1F, 0.05F, 0.85F},
	/* 7  Emerald orbiter   */ {{ 4, 0.5F, 0},   { 0, 0, 1},     4,   0.35F, 0.6F, {0.15F,0.85F,0.30F},0.8F, 0.15F, 1.00F},
	/* 8  Rose quartz       */ {{ 0,-10, 2},     { 1, 0, 0},    10,   0.55F, 1.8F, {0.90F,0.55F,0.65F},0.3F, 0.25F, 0.95F},
	/* 9  Deep blue         */ {{ 0, 0,-3.5F},   { 0, 1, 0},     3.5F,0.30F, 0.4F, {0.10F,0.25F,0.95F},0.9F, 0.10F, 1.00F},
	/* 10 Amber planet      */ {{ 9,-2, 0},      { 0, 1, 0},     9,   0.65F, 3.0F, {1.0F,0.60F,0.10F}, 0.7F, 0.30F, 1.00F},
	/* 11 Violet comet      */ {{11, 2,-3},      {-0.4F, 0, 0.7F},11,  0.30F, 0.4F, {0.55F,0.15F,0.95F},0.2F, 0.08F, 0.92F},
	/* 12 Turquoise moon    */ {{-6, 0,-1},      { 0, 1, 0},     6,   0.45F, 1.2F, {0.20F,0.80F,0.75F},0.6F, 0.12F, 1.00F},
	/* 13 Crimson dwarf     */ {{-12,-2, 1},     { 0.3F, 0.7F, 0},12,  0.50F, 2.5F, {0.90F,0.12F,0.15F},0.85F,0.35F, 0.95F},
};
/* clang-format on */

static const int ORBITER_COUNT = (int)(sizeof(ORBITERS) / sizeof(ORBITERS[0]));

void nbody_init_preset(NBodySim* sim)
{
	(void)safe_memset(sim, sizeof(*sim), 0, sizeof(*sim));
	sim->gravity = NBODY_DEFAULT_G;
	sim->time_scale = 1.0F;
	sim->target_time_scale = 1.0F;
	sim->paused = false;

	/* Body 0: Central star — gold/bronze, heavy, stationary */
	add_body(sim, (vec3){0}, (vec3){0}, CENTRAL_STAR_MASS,
	         CENTRAL_STAR_RADIUS, CENTRAL_STAR_ALBEDO,
	         CENTRAL_STAR_METALLIC, CENTRAL_STAR_ROUGHNESS);

	/* Add all orbiters from the table. */
	for (int idx = 0; idx < ORBITER_COUNT; idx++) {
		const struct OrbiterDef* orb = &ORBITERS[idx];
		float spd = softened_orbital_vel(
		                sim->gravity, CENTRAL_STAR_MASS, orb->orbit_r,
		                CENTRAL_STAR_RADIUS, orb->body_r) *
		            orb->ecc;
		vec3 normalized_vel_dir;
		glm_vec3_copy((float*)orb->vel_dir, normalized_vel_dir);
		glm_vec3_normalize(normalized_vel_dir);

		vec3 vel = {normalized_vel_dir[0] * spd,
		            normalized_vel_dir[1] * spd,
		            normalized_vel_dir[2] * spd};
		add_body(sim, orb->pos, vel, orb->mass, orb->body_r,
		         orb->albedo, orb->metallic, orb->roughness);
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

	/* Snapshot total energy as reference for stability diagnostics. */
	sim->initial_energy = nbody_total_energy(sim);

	/* Initialize prev_position = position for first frame (no motion). */
	for (int i = 0; i < sim->body_count; i++) {
		glm_vec3_copy(sim->bodies[i].position,
		              sim->bodies[i].prev_position);
	}
}

/* ========================================================================= */
/* Energy diagnostics                                                        */
/* ========================================================================= */

/* E = Σ ½ m v² − Σ_{i<j} G m_i m_j / sqrt(r² + ε²)
 * Conserved (bounded oscillation) by the symplectic integrator. */

float nbody_total_energy(const NBodySim* sim)
{
	float kinetic = 0.0F;
	for (int i = 0; i < sim->body_count; i++) {
		vec3 vel;
		glm_vec3_copy((float*)sim->bodies[i].velocity, vel);
		kinetic += HALF * sim->bodies[i].mass * glm_vec3_dot(vel, vel);
	}

	float potential = 0.0F;
	for (int i = 0; i < sim->body_count; i++) {
		for (int j = i + 1; j < sim->body_count; j++) {
			vec3 diff;
			vec3 pos_i;
			vec3 pos_j;
			glm_vec3_copy((float*)sim->bodies[i].position, pos_i);
			glm_vec3_copy((float*)sim->bodies[j].position, pos_j);
			glm_vec3_sub(pos_j, pos_i, diff);
			float eps2 = pair_softening_sq(sim->bodies[i].radius,
			                               sim->bodies[j].radius);
			float dist = sqrtf(glm_vec3_dot(diff, diff) + eps2);
			potential -= sim->gravity * sim->bodies[i].mass *
			             sim->bodies[j].mass / dist;
		}
	}

	/* Confinement potential: V = ½k(r - r_max)² for r > r_max.
	 * Must match the force in compute_accelerations() for energy
	 * conservation diagnostics to remain accurate. */
	for (int i = 1; i < sim->body_count; i++) {
		vec3 rel;
		vec3 pos_i;
		vec3 pos_star;
		glm_vec3_copy((float*)sim->bodies[i].position, pos_i);
		glm_vec3_copy((float*)sim->bodies[0].position, pos_star);
		glm_vec3_sub(pos_i, pos_star, rel);
		float dist = glm_vec3_norm(rel);
		if (dist > NBODY_CONFINEMENT_RADIUS) {
			float overshoot = dist - NBODY_CONFINEMENT_RADIUS;
			potential +=
			    HALF * NBODY_CONFINEMENT_K * overshoot * overshoot;
		}
	}

	return kinetic + potential;
}

/* ========================================================================= */
/* Gravity & Integration                                                     */
/* ========================================================================= */

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
			vec3 pos_i;
			vec3 pos_j;
			glm_vec3_copy((float*)sim->bodies[i].position, pos_i);
			glm_vec3_copy((float*)sim->bodies[j].position, pos_j);
			glm_vec3_sub(pos_j, pos_i, diff);

			float eps2 = pair_softening_sq(sim->bodies[i].radius,
			                               sim->bodies[j].radius);
			float dist_sq = glm_vec3_dot(diff, diff) + eps2;
			float inv_dist = 1.0F / sqrtf(dist_sq);
			float inv_dist3 = inv_dist * inv_dist * inv_dist;
			float grav = sim->gravity * inv_dist3;

			vec3 force;
			glm_vec3_scale(diff, grav * sim->bodies[j].mass, force);
			glm_vec3_add(accel[i], force, accel[i]);

			glm_vec3_scale(diff, grav * sim->bodies[i].mass, force);
			glm_vec3_sub(accel[j], force, accel[j]);
		}
	}

	/* Confinement potential: F = -k·(r - r_max)·r̂  for r > r_max.
	 * Measured from body[0] (central star). Newton's 3rd law: the
	 * reaction force is applied to the star so momentum is conserved.
	 * The acceleration is mass-independent (V ∝ m), so the reaction
	 * on the star is scaled by mass_i / mass_0.
	 * Conservative → Verlet stays symplectic. */
	for (int i = 1; i < sim->body_count; i++) {
		vec3 rel;
		vec3 pos_i;
		vec3 pos_star;
		glm_vec3_copy((float*)sim->bodies[i].position, pos_i);
		glm_vec3_copy((float*)sim->bodies[0].position, pos_star);
		glm_vec3_sub(pos_i, pos_star, rel);
		float dist = glm_vec3_norm(rel);
		if (dist > NBODY_CONFINEMENT_RADIUS) {
			float overshoot = dist - NBODY_CONFINEMENT_RADIUS;
			float strength =
			    -NBODY_CONFINEMENT_K * overshoot / dist;
			vec3 confine_accel;
			glm_vec3_scale(rel, strength, confine_accel);
			glm_vec3_add(accel[i], confine_accel, accel[i]);
			/* Newton 3rd law: reaction on star, mass-weighted. */
			float ratio = sim->bodies[i].mass / sim->bodies[0].mass;
			vec3 reaction;
			glm_vec3_scale(confine_accel, ratio, reaction);
			glm_vec3_sub(accel[0], reaction, accel[0]);
		}
	}
}

/* Velocity Verlet (symplectic, 2nd order).
 * Energy oscillates with bounded amplitude thanks to Plummer softening. */

static void integrate_step(NBodySim* sim, float delta_time)
{
	vec3 accel_old[NBODY_MAX_BODIES];
	vec3 accel_new[NBODY_MAX_BODIES];

	compute_accelerations(sim, accel_old);

	/* x += v·dt + ½·a·dt² */
	for (int i = 0; i < sim->body_count; i++) {
		vec3 tmp;
		glm_vec3_scale(sim->bodies[i].velocity, delta_time, tmp);
		glm_vec3_add(sim->bodies[i].position, tmp,
		             sim->bodies[i].position);
		glm_vec3_scale(accel_old[i], HALF * delta_time * delta_time,
		               tmp);
		glm_vec3_add(sim->bodies[i].position, tmp,
		             sim->bodies[i].position);
	}

	compute_accelerations(sim, accel_new);

	/* v += ½·(a_old + a_new)·dt */
	for (int i = 0; i < sim->body_count; i++) {
		vec3 avg;
		glm_vec3_add(accel_old[i], accel_new[i], avg);
		glm_vec3_scale(avg, HALF * delta_time, avg);
		glm_vec3_add(sim->bodies[i].velocity, avg,
		             sim->bodies[i].velocity);

		if (isnan(sim->bodies[i].velocity[0])) {
			printf(
			    "!!! Body %d velocity became NaN at step! "
			    "v_prev=%f a_old=%f a_new=%f dt=%f\n",
			    i, (double)sim->bodies[i].velocity[0],
			    (double)accel_old[i][0], (double)accel_new[i][0],
			    (double)delta_time);
		}
	}

	/* Radial damping in the confinement zone — proportional to overshoot.
	 * γ_eff = γ · (r - r_max) / r_max : near the boundary the damping
	 * is negligible, far beyond it the damping is strong.  This lets
	 * the system reach an equilibrium where orbits no longer cross
	 * the boundary and energy drain stops (drift plateaus).
	 * Only the outward radial component is damped; tangential speed
	 * is preserved → angular momentum conserved.
	 * Momentum conservation: impulse transferred to star (body 0). */
	for (int i = 1; i < sim->body_count; i++) {
		vec3 rel;
		glm_vec3_sub(sim->bodies[i].position, sim->bodies[0].position,
		             rel);
		float dist = glm_vec3_norm(rel);
		if (dist > NBODY_CONFINEMENT_RADIUS && dist > EPSILON) {
			/* Record impact for VFX — keep peak velocity per body
			 */
			float v_out =
			    glm_vec3_dot(sim->bodies[i].velocity, rel) / dist;
			if (v_out > sim->impacts[i].velocity) {
				glm_vec3_copy(sim->bodies[i].position,
				              sim->impacts[i].position);
				glm_vec3_copy(sim->bodies[i].albedo,
				              sim->impacts[i].color);
				sim->impacts[i].velocity = v_out;
				sim->impacts[i].active = true;
			}

			vec3 r_hat;
			glm_vec3_scale(rel, 1.0F / dist, r_hat);
			float v_radial =
			    glm_vec3_dot(sim->bodies[i].velocity, r_hat);
			if (v_radial > 0.0F) { /* outward only */
				float overshoot =
				    dist - NBODY_CONFINEMENT_RADIUS;
				float damp =
				    NBODY_CONFINEMENT_DAMPING *
				    (overshoot / NBODY_CONFINEMENT_RADIUS) *
				    delta_time;
				if (damp > 1.0F) {
					damp = 1.0F;
				}
				vec3 vel_damp;
				glm_vec3_scale(r_hat, -v_radial * damp,
				               vel_damp);
				glm_vec3_add(sim->bodies[i].velocity, vel_damp,
				             sim->bodies[i].velocity);
				/* Momentum conservation: Δp_star = -Δp_i
				 * → Δv_star = -(m_i/m_0)·dv */
				float ratio =
				    sim->bodies[i].mass / sim->bodies[0].mass;
				vec3 dv_star;
				glm_vec3_scale(vel_damp, -ratio, dv_star);
				glm_vec3_add(sim->bodies[0].velocity, dv_star,
				             sim->bodies[0].velocity);
			}
		}
	}
}

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

	float scaled_dt = delta_time * sim->time_scale;
	sim->accumulator += scaled_dt;

	/* Clear per-body impact flags from previous frame */
	for (int i = 0; i < sim->body_count; i++) {
		sim->impacts[i].active = false;
		sim->impacts[i].velocity = 0.0F;
	}

	/* Clamp magnitude to prevent spiral of death */
	if (sim->accumulator > NBODY_MAX_ACCUMULATOR) {
		sim->accumulator = NBODY_MAX_ACCUMULATOR;
	} else if (sim->accumulator < -NBODY_MAX_ACCUMULATOR) {
		sim->accumulator = -NBODY_MAX_ACCUMULATOR;
	}

	/* Integrate: step sign follows accumulator sign (time reversal) */
	float step = copysignf(NBODY_FIXED_DT, sim->accumulator);
	while (fabsf(sim->accumulator) >= NBODY_FIXED_DT) {
		integrate_step(sim, step);
		sim->accumulator -= step;
		sim->sim_time += step;
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

float nbody_kinetic_energy(const NBodySim* sim)
{
	float kinetic = 0.0F;
	for (int i = 0; i < sim->body_count; i++) {
		vec3 vel;
		glm_vec3_copy((float*)sim->bodies[i].velocity, vel);
		kinetic += HALF * sim->bodies[i].mass * glm_vec3_dot(vel, vel);
	}
	return kinetic;
}

float nbody_energy_drift(const NBodySim* sim)
{
	float ref_energy = sim->initial_energy;
	if (ref_energy == 0.0F) {
		return 0.0F;
	}
	float current = nbody_total_energy(sim);
	float diff = current - ref_energy;
	return (diff < 0.0F ? -diff : diff) /
	       (ref_energy < 0.0F ? -ref_energy : ref_energy);
}

float nbody_energy_drift_signed(const NBodySim* sim)
{
	float ref_energy = sim->initial_energy;
	if (ref_energy == 0.0F) {
		return 0.0F;
	}
	float current = nbody_total_energy(sim);
	/* For negative E0 (bound systems), energy becoming more negative
	 * means energy was lost (damping) → report as negative drift. */
	float diff = current - ref_energy;
	float abs_ref = ref_energy < 0.0F ? -ref_energy : ref_energy;
	return diff / abs_ref;
}

void nbody_update_time_scale(NBodySim* sim, float delta_time)
{
	float diff = sim->target_time_scale - sim->time_scale;
	if (diff == 0.0F) {
		return;
	}
	float step = NBODY_TIME_SCALE_RATE * delta_time;
	if (fabsf(diff) <= step) {
		sim->time_scale = sim->target_time_scale;
	} else {
		sim->time_scale += copysignf(step, diff);
	}
}
