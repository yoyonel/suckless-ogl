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

static void add_body(NBodySim* sim, const double pos[3], const double vel[3],
                     double mass, float radius, const vec3 albedo,
                     float metallic, float roughness)
{
	if (sim->body_count >= NBODY_MAX_BODIES) {
		return;
	}
	NBodyParticle* body = &sim->bodies[sim->body_count];
	body->position[0] = pos[0];
	body->position[1] = pos[1];
	body->position[2] = pos[2];
	body->velocity[0] = vel[0];
	body->velocity[1] = vel[1];
	body->velocity[2] = vel[2];
	body->mass = mass;
	body->radius = radius;
	glm_vec3_copy((float*)albedo, body->albedo);
	body->metallic = metallic;
	body->roughness = roughness;
	sim->body_count++;
}

/* Per-pair Plummer softening: ε² = max(NBODY_SOFTENING_SQ, (F·(r_i+r_j))²).
 * Used by forces, energy, and initial velocities for consistency. */
static double pair_softening_sq(double radius_i, double radius_j)
{
	double sum_r = radius_i + radius_j;
	double eps = (double)NBODY_SOFTENING_FACTOR * sum_r;
	double eps_sq = eps * eps;
	return eps_sq > (double)NBODY_SOFTENING_SQ ? eps_sq
	                                           : (double)NBODY_SOFTENING_SQ;
}

/* Circular-orbit speed in a Plummer-softened potential:
 * v = r · sqrt(G·M / (r² + ε²)^{3/2})  */
static double softened_orbital_vel(double grav, double central_mass,
                                   double orbit_r, double star_r, double body_r)
{
	double eps_sq = pair_softening_sq(star_r, body_r);
	double r_sq = orbit_r * orbit_r;
	double d_sq = r_sq + eps_sq;
	double d_32 = d_sq * sqrt(d_sq);
	return orbit_r * sqrt(grav * central_mass / d_32);
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
	add_body(sim, (double[]){0, 0, 0}, (double[]){0, 0, 0},
	         (double)CENTRAL_STAR_MASS, CENTRAL_STAR_RADIUS,
	         CENTRAL_STAR_ALBEDO, CENTRAL_STAR_METALLIC,
	         CENTRAL_STAR_ROUGHNESS);

	/* Add all orbiters from the table. */
	for (int idx = 0; idx < ORBITER_COUNT; idx++) {
		const struct OrbiterDef* orb = &ORBITERS[idx];
		double spd = softened_orbital_vel((double)sim->gravity,
		                                  (double)CENTRAL_STAR_MASS,
		                                  (double)orb->orbit_r,
		                                  (double)CENTRAL_STAR_RADIUS,
		                                  (double)orb->body_r) *
		             (double)orb->ecc;

		double normalized_vel_dir[3] = {(double)orb->vel_dir[0],
		                                (double)orb->vel_dir[1],
		                                (double)orb->vel_dir[2]};
		double mag =
		    sqrt(normalized_vel_dir[0] * normalized_vel_dir[0] +
		         normalized_vel_dir[1] * normalized_vel_dir[1] +
		         normalized_vel_dir[2] * normalized_vel_dir[2]);
		if (mag > 0.0) {
			normalized_vel_dir[0] /= mag;
			normalized_vel_dir[1] /= mag;
			normalized_vel_dir[2] /= mag;
		}

		double vel[3] = {normalized_vel_dir[0] * spd,
		                 normalized_vel_dir[1] * spd,
		                 normalized_vel_dir[2] * spd};
		add_body(sim,
		         (double[]){(double)orb->pos[0], (double)orb->pos[1],
		                    (double)orb->pos[2]},
		         vel, (double)orb->mass, orb->body_r, orb->albedo,
		         orb->metallic, orb->roughness);
	}

	/* Zero the total momentum so the center of mass stays fixed.
	 * v_cm = Σ(m_i * v_i) / Σ(m_i), then subtract from each body. */
	double total_momentum[3] = {0.0, 0.0, 0.0};
	double total_mass = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		total_momentum[0] +=
		    sim->bodies[i].velocity[0] * sim->bodies[i].mass;
		total_momentum[1] +=
		    sim->bodies[i].velocity[1] * sim->bodies[i].mass;
		total_momentum[2] +=
		    sim->bodies[i].velocity[2] * sim->bodies[i].mass;
		total_mass += sim->bodies[i].mass;
	}
	double vel_cm[3] = {total_momentum[0] / total_mass,
	                    total_momentum[1] / total_mass,
	                    total_momentum[2] / total_mass};
	for (int i = 0; i < sim->body_count; i++) {
		sim->bodies[i].velocity[0] -= vel_cm[0];
		sim->bodies[i].velocity[1] -= vel_cm[1];
		sim->bodies[i].velocity[2] -= vel_cm[2];
	}

	/* Snapshot total energy as reference for stability diagnostics. */
	sim->initial_energy = nbody_total_energy(sim);

	/* Initialize prev_position = position for first frame (no motion). */
	for (int i = 0; i < sim->body_count; i++) {
		sim->bodies[i].prev_position[0] = sim->bodies[i].position[0];
		sim->bodies[i].prev_position[1] = sim->bodies[i].position[1];
		sim->bodies[i].prev_position[2] = sim->bodies[i].position[2];
	}
}

/* ========================================================================= */
/* Energy diagnostics                                                        */
/* ========================================================================= */

/* E = Σ ½ m v² − Σ_{i<j} G m_i m_j / sqrt(r² + ε²)
 * Conserved (bounded oscillation) by the symplectic integrator. */

float nbody_total_energy(const NBodySim* sim)
{
	double kinetic = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		double vx = sim->bodies[i].velocity[0];
		double vy = sim->bodies[i].velocity[1];
		double vz = sim->bodies[i].velocity[2];
		kinetic +=
		    0.5 * sim->bodies[i].mass * (vx * vx + vy * vy + vz * vz);
	}

	double potential = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		for (int j = i + 1; j < sim->body_count; j++) {
			double dx = sim->bodies[j].position[0] -
			            sim->bodies[i].position[0];
			double dy = sim->bodies[j].position[1] -
			            sim->bodies[i].position[1];
			double dz = sim->bodies[j].position[2] -
			            sim->bodies[i].position[2];

			double eps2 = pair_softening_sq(sim->bodies[i].radius,
			                                sim->bodies[j].radius);
			double dist = sqrt(dx * dx + dy * dy + dz * dz + eps2);
			potential -= (double)sim->gravity *
			             sim->bodies[i].mass * sim->bodies[j].mass /
			             dist;
		}
	}

	/* Confinement potential: V = ½k(r - r_max)² for r > r_max.
	 * Must match the force in compute_accelerations() for energy
	 * conservation diagnostics to remain accurate. */
	for (int i = 1; i < sim->body_count; i++) {
		double rx =
		    sim->bodies[i].position[0] - sim->bodies[0].position[0];
		double ry =
		    sim->bodies[i].position[1] - sim->bodies[0].position[1];
		double rz =
		    sim->bodies[i].position[2] - sim->bodies[0].position[2];
		double dist = sqrt(rx * rx + ry * ry + rz * rz);
		if (dist > (double)NBODY_CONFINEMENT_RADIUS) {
			double overshoot =
			    dist - (double)NBODY_CONFINEMENT_RADIUS;
			potential += 0.5 * (double)NBODY_CONFINEMENT_K *
			             overshoot * overshoot;
		}
	}

	return (float)(kinetic + potential);
}

/* ========================================================================= */
/* Gravity & Integration                                                     */
/* ========================================================================= */

/* O(N²) pairwise gravity with per-pair Plummer softening.
 * ε² = max(NBODY_SOFTENING_SQ, (F·(r_i+r_j))²)  */

static void compute_accelerations(const NBodySim* sim, double accel[][3])
{
	for (int i = 0; i < sim->body_count; i++) {
		accel[i][0] = 0.0;
		accel[i][1] = 0.0;
		accel[i][2] = 0.0;
	}

	for (int i = 0; i < sim->body_count; i++) {
		for (int j = i + 1; j < sim->body_count; j++) {
			double dx = sim->bodies[j].position[0] -
			            sim->bodies[i].position[0];
			double dy = sim->bodies[j].position[1] -
			            sim->bodies[i].position[1];
			double dz = sim->bodies[j].position[2] -
			            sim->bodies[i].position[2];

			double eps2 = pair_softening_sq(sim->bodies[i].radius,
			                                sim->bodies[j].radius);
			double dist_sq = dx * dx + dy * dy + dz * dz + eps2;
			double inv_dist = 1.0 / sqrt(dist_sq);
			double inv_dist3 = inv_dist * inv_dist * inv_dist;
			double grav = (double)sim->gravity * inv_dist3;

			double fx = dx * grav * sim->bodies[j].mass;
			double fy = dy * grav * sim->bodies[j].mass;
			double fz = dz * grav * sim->bodies[j].mass;
			accel[i][0] += fx;
			accel[i][1] += fy;
			accel[i][2] += fz;

			fx = dx * grav * sim->bodies[i].mass;
			fy = dy * grav * sim->bodies[i].mass;
			fz = dz * grav * sim->bodies[i].mass;
			accel[j][0] -= fx;
			accel[j][1] -= fy;
			accel[j][2] -= fz;
		}
	}

	/* Confinement potential: F = -k·(r - r_max)·r̂  for r > r_max.
	 * Measured from body[0] (central star). Newton's 3rd law: the
	 * reaction force is applied to the star so momentum is conserved.
	 * The acceleration is mass-independent (V ∝ m), so the reaction
	 * on the star is scaled by mass_i / mass_0.
	 * Conservative → Verlet stays symplectic. */
	for (int i = 1; i < sim->body_count; i++) {
		double rx =
		    sim->bodies[i].position[0] - sim->bodies[0].position[0];
		double ry =
		    sim->bodies[i].position[1] - sim->bodies[0].position[1];
		double rz =
		    sim->bodies[i].position[2] - sim->bodies[0].position[2];
		double dist = sqrt(rx * rx + ry * ry + rz * rz);
		if (dist > (double)NBODY_CONFINEMENT_RADIUS) {
			double overshoot =
			    dist - (double)NBODY_CONFINEMENT_RADIUS;
			double strength =
			    -(double)NBODY_CONFINEMENT_K * overshoot / dist;
			double ax = rx * strength;
			double ay = ry * strength;
			double az = rz * strength;
			accel[i][0] += ax;
			accel[i][1] += ay;
			accel[i][2] += az;
			/* Newton 3rd law: reaction on star, mass-weighted. */
			double ratio =
			    sim->bodies[i].mass / sim->bodies[0].mass;
			accel[0][0] -= ax * ratio;
			accel[0][1] -= ay * ratio;
			accel[0][2] -= az * ratio;
		}
	}
}

/* Velocity Verlet (symplectic, 2nd order).
 * Energy oscillates with bounded amplitude thanks to Plummer softening. */

static void integrate_step(NBodySim* sim, float delta_time)
{
	double accel_old[NBODY_MAX_BODIES][3];
	double accel_new[NBODY_MAX_BODIES][3];
	double dt = (double)delta_time;

	compute_accelerations(sim, accel_old);

	/* x += v·dt + ½·a·dt² */
	for (int i = 0; i < sim->body_count; i++) {
		sim->bodies[i].position[0] += sim->bodies[i].velocity[0] * dt +
		                              0.5 * accel_old[i][0] * dt * dt;
		sim->bodies[i].position[1] += sim->bodies[i].velocity[1] * dt +
		                              0.5 * accel_old[i][1] * dt * dt;
		sim->bodies[i].position[2] += sim->bodies[i].velocity[2] * dt +
		                              0.5 * accel_old[i][2] * dt * dt;
	}

	compute_accelerations(sim, accel_new);

	/* v += ½·(a_old + a_new)·dt */
	for (int i = 0; i < sim->body_count; i++) {
		sim->bodies[i].velocity[0] +=
		    0.5 * (accel_old[i][0] + accel_new[i][0]) * dt;
		sim->bodies[i].velocity[1] +=
		    0.5 * (accel_old[i][1] + accel_new[i][1]) * dt;
		sim->bodies[i].velocity[2] +=
		    0.5 * (accel_old[i][2] + accel_new[i][2]) * dt;

		if (isnan(sim->bodies[i].velocity[0])) {
			printf("!!! Body %d velocity became NaN at step!\n", i);
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
		double rx =
		    sim->bodies[i].position[0] - sim->bodies[0].position[0];
		double ry =
		    sim->bodies[i].position[1] - sim->bodies[0].position[1];
		double rz =
		    sim->bodies[i].position[2] - sim->bodies[0].position[2];
		double dist = sqrt(rx * rx + ry * ry + rz * rz);
		if (dist > (double)NBODY_CONFINEMENT_RADIUS && dist > 1e-9) {
			/* Record impact for VFX — keep peak velocity per body
			 */
			double v_out = (sim->bodies[i].velocity[0] * rx +
			                sim->bodies[i].velocity[1] * ry +
			                sim->bodies[i].velocity[2] * rz) /
			               dist;
			if (v_out > (double)sim->impacts[i].velocity) {
				sim->impacts[i].position[0] =
				    (float)sim->bodies[i].position[0];
				sim->impacts[i].position[1] =
				    (float)sim->bodies[i].position[1];
				sim->impacts[i].position[2] =
				    (float)sim->bodies[i].position[2];
				glm_vec3_copy(sim->bodies[i].albedo,
				              sim->impacts[i].color);
				sim->impacts[i].velocity = (float)v_out;
				sim->impacts[i].active = true;
			}

			double r_hat_x = rx / dist;
			double r_hat_y = ry / dist;
			double r_hat_z = rz / dist;
			double v_radial = sim->bodies[i].velocity[0] * r_hat_x +
			                  sim->bodies[i].velocity[1] * r_hat_y +
			                  sim->bodies[i].velocity[2] * r_hat_z;
			if (v_radial > 0.0) { /* outward only */
				double overshoot =
				    dist - (double)NBODY_CONFINEMENT_RADIUS;
				/* Stability fix: use abs(delta_time) for
				 * damping to ensure energy is always removed
				 * even in reverse. */
				double damp =
				    (double)NBODY_CONFINEMENT_DAMPING *
				    (overshoot /
				     (double)NBODY_CONFINEMENT_RADIUS) *
				    fabs((double)delta_time);
				if (damp > 1.0) {
					damp = 1.0;
				}
				double dvx = -v_radial * damp * r_hat_x;
				double dvy = -v_radial * damp * r_hat_y;
				double dvz = -v_radial * damp * r_hat_z;
				sim->bodies[i].velocity[0] += dvx;
				sim->bodies[i].velocity[1] += dvy;
				sim->bodies[i].velocity[2] += dvz;
				/* Momentum conservation: Δp_star = -Δp_i
				 * → Δv_star = -(m_i/m_0)·dv */
				double ratio =
				    sim->bodies[i].mass / sim->bodies[0].mass;
				sim->bodies[0].velocity[0] -= dvx * ratio;
				sim->bodies[0].velocity[1] -= dvy * ratio;
				sim->bodies[0].velocity[2] -= dvz * ratio;
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
		sim->bodies[i].prev_position[0] = sim->bodies[i].position[0];
		sim->bodies[i].prev_position[1] = sim->bodies[i].position[1];
		sim->bodies[i].prev_position[2] = sim->bodies[i].position[2];
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
		vec3 pos_f = {(float)body->position[0],
		              (float)body->position[1],
		              (float)body->position[2]};
		glm_translate(out[i].model, pos_f);
		vec3 scale = {body->radius, body->radius, body->radius};
		glm_scale(out[i].model, scale);
		glm_vec3_copy((float*)body->albedo, out[i].albedo);
		out[i].metallic = body->metallic;
		out[i].roughness = body->roughness;
		out[i].ao = 1.0F;
		out[i].prev_center[0] = (float)body->prev_position[0];
		out[i].prev_center[1] = (float)body->prev_position[1];
		out[i].prev_center[2] = (float)body->prev_position[2];
	}
}

int nbody_get_count(const NBodySim* sim)
{
	return sim->body_count;
}

float nbody_kinetic_energy(const NBodySim* sim)
{
	double kinetic = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		double vx = sim->bodies[i].velocity[0];
		double vy = sim->bodies[i].velocity[1];
		double vz = sim->bodies[i].velocity[2];
		kinetic +=
		    0.5 * sim->bodies[i].mass * (vx * vx + vy * vy + vz * vz);
	}
	return (float)kinetic;
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
