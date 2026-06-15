#include "dvec3.h"
#include "nbody.h"
#include "nbody_internal.h"
#include <math.h>

float nbody_total_energy(const NBodySim* sim)
{
	double kinetic = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		kinetic += HALF * sim->bodies[i].mass *
		           dvec3_norm2(sim->bodies[i].velocity);
	}

	double potential = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		for (int j = i + 1; j < sim->body_count; j++) {
			dvec3 diff;
			dvec3_sub(sim->bodies[j].position,
			          sim->bodies[i].position, diff);

			double eps2 = pair_softening_sq(sim->bodies[i].radius,
			                                sim->bodies[j].radius);
			double dist = sqrt(dvec3_norm2(diff) + eps2);
			potential -= (double)sim->gravity *
			             sim->bodies[i].mass * sim->bodies[j].mass /
			             dist;
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
			potential += HALF * (double)NBODY_CONFINEMENT_K *
			             overshoot * overshoot;
		}
	}

	return (float)(kinetic + potential);
}

float nbody_kinetic_energy(const NBodySim* sim)
{
	double kinetic = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		kinetic += HALF * sim->bodies[i].mass *
		           dvec3_norm2(sim->bodies[i].velocity);
	}
	return (float)kinetic;
}

float nbody_energy_drift(const NBodySim* sim)
{
	float ref_energy = sim->initial_energy;
	if (ref_energy == 0.0F)
		return 0.0F;

	float current = nbody_total_energy(sim);
	float diff = current - ref_energy;
	return (diff < 0.0F ? -diff : diff) /
	       (ref_energy < 0.0F ? -ref_energy : ref_energy);
}

float nbody_energy_drift_signed(const NBodySim* sim)
{
	float ref_energy = sim->initial_energy;
	if (ref_energy == 0.0F)
		return 0.0F;

	float current = nbody_total_energy(sim);
	float diff = current - ref_energy;
	float abs_ref = ref_energy < 0.0F ? -ref_energy : ref_energy;
	return diff / abs_ref;
}
