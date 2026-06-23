#include "dvec3.h"
#include "nbody.h"
#include "nbody_internal.h"
#include "nbody_types.h"
#include <cglm/vec3.h>
#include <math.h>
#include <stdio.h>

static const double MIN_DIST = 1e-9;

static void compute_accelerations(const NBodySim* sim, double accel[][3])
{
	for (int i = 0; i < sim->body_count; i++) {
		dvec3_zero(accel[i]);
	}

	for (int i = 0; i < sim->body_count; i++) {
		for (int j = i + 1; j < sim->body_count; j++) {
			dvec3 diff;
			dvec3_sub(sim->bodies[j].position,
			          sim->bodies[i].position, diff);

			double eps2 = pair_softening_sq(sim->bodies[i].radius,
			                                sim->bodies[j].radius);
			double dist_sq = dvec3_norm2(diff) + eps2;
			double inv_dist = 1.0 / sqrt(dist_sq);
			double inv_dist3 = inv_dist * inv_dist * inv_dist;
			double grav = (double)sim->gravity * inv_dist3;

			dvec3 force;
			dvec3_scale(diff, grav * sim->bodies[j].mass, force);
			dvec3_addto(accel[i], force);

			dvec3_scale(diff, grav * sim->bodies[i].mass, force);
			dvec3_subfrom(accel[j], force);
		}
	}

	for (int i = 1; i < sim->body_count; i++) {
		dvec3 rel;
		dvec3_sub(sim->bodies[i].position, sim->bodies[0].position,
		          rel);
		double dist = dvec3_norm(rel);
		if (dist > (double)NBODY_CONFINEMENT_RADIUS) {
			double overshoot =
			    dist - (double)NBODY_CONFINEMENT_RADIUS;
			double strength =
			    -(double)NBODY_CONFINEMENT_K * overshoot / dist;

			dvec3 acc;
			dvec3_scale(rel, strength, acc);
			dvec3_addto(accel[i], acc);

			double ratio =
			    sim->bodies[i].mass / sim->bodies[0].mass;
			dvec3 reaction;
			dvec3_scale(acc, ratio, reaction);
			dvec3_subfrom(accel[0], reaction);
		}
	}
}

static void integrate_step(NBodySim* sim, float delta_time)
{
	double accel_old[NBODY_MAX_BODIES][3];
	double accel_new[NBODY_MAX_BODIES][3];
	double delta_t = (double)delta_time;

	compute_accelerations(sim, accel_old);

	for (int i = 0; i < sim->body_count; i++) {
		dvec3_muladds(sim->bodies[i].position, sim->bodies[i].velocity,
		              delta_t);
		dvec3_muladds(sim->bodies[i].position, accel_old[i],
		              HALF * delta_t * delta_t);
	}

	compute_accelerations(sim, accel_new);

	for (int i = 0; i < sim->body_count; i++) {
		dvec3 accel_avg;
		dvec3_add(accel_old[i], accel_new[i], accel_avg);
		dvec3_muladds(sim->bodies[i].velocity, accel_avg,
		              HALF * delta_t);

		if (isnan(sim->bodies[i].velocity[0])) {
			printf("!!! Body %d velocity became NaN at step!\n", i);
		}
	}

	for (int i = 1; i < sim->body_count; i++) {
		dvec3 rel;
		dvec3_sub(sim->bodies[i].position, sim->bodies[0].position,
		          rel);
		double dist = dvec3_norm(rel);

		if (dist > (double)NBODY_CONFINEMENT_RADIUS &&
		    dist > MIN_DIST) {
			double v_out =
			    dvec3_dot(sim->bodies[i].velocity, rel) / dist;

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

			dvec3 r_hat;
			dvec3_scale(rel, 1.0 / dist, r_hat);
			double v_radial =
			    dvec3_dot(sim->bodies[i].velocity, r_hat);

			if (v_radial > 0.0) {
				double overshoot =
				    dist - (double)NBODY_CONFINEMENT_RADIUS;
				double damp =
				    (double)NBODY_CONFINEMENT_DAMPING *
				    (overshoot /
				     (double)NBODY_CONFINEMENT_RADIUS) *
				    fabs((double)delta_time);
				if (damp > 1.0) {
					damp = 1.0;
				}

				dvec3 delta_v;
				dvec3_scale(r_hat, -v_radial * damp, delta_v);
				dvec3_addto(sim->bodies[i].velocity, delta_v);

				double ratio =
				    sim->bodies[i].mass / sim->bodies[0].mass;
				dvec3 star_dv;
				dvec3_scale(delta_v, ratio, star_dv);
				dvec3_subfrom(sim->bodies[0].velocity, star_dv);
			}
		}
	}
}

void nbody_step(NBodySim* sim, float delta_time)
{
	if (sim->paused) {
		return;
	}

	for (int i = 0; i < sim->body_count; i++) {
		dvec3_copy(sim->bodies[i].position,
		           sim->bodies[i].prev_position);
	}

	float scaled_dt = delta_time * sim->time_scale;
	sim->accumulator += scaled_dt;

	for (int i = 0; i < sim->body_count; i++) {
		sim->impacts[i].active = false;
		sim->impacts[i].velocity = 0.0F;
	}

	if (sim->accumulator > NBODY_MAX_ACCUMULATOR) {
		sim->accumulator = NBODY_MAX_ACCUMULATOR;
	} else if (sim->accumulator < -NBODY_MAX_ACCUMULATOR) {
		sim->accumulator = -NBODY_MAX_ACCUMULATOR;
	}

	float step = copysignf(NBODY_FIXED_DT, sim->accumulator);
	while (fabsf(sim->accumulator) >= NBODY_FIXED_DT) {
		integrate_step(sim, step);
		sim->accumulator -= step;
		sim->sim_time += step;
	}
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

int nbody_get_count(const NBodySim* sim)
{
	return sim->body_count;
}
