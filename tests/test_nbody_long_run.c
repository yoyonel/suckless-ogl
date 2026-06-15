/**
 * @file test_nbody_long_run.c
 * @brief Test fonctionnel de stabilité à long terme (1200s simulées).
 */

#include "nbody.h"
#include "unity.h"
#include <math.h>
#include <stdio.h>

static const float SIM_DURATION = 1200.0F;
static const float MAX_ENERGY_DRIFT = 0.06F;
static const float MAX_COM_DRIFT = 0.5F;
static const float MAX_BODY_DISTANCE = 50.0F;
static const float STEP_DT = 1.0F / 60.0F;
static const float REPORT_INTERVAL = 60.0F;

enum { MSG_BUF_SIZE = 80 };

void setUp(void)
{
}

void tearDown(void)
{
}

static double vec3_length(const double v[3])
{
	return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static void compute_center_of_mass(const NBodySim* sim, double out[3])
{
	out[0] = 0.0;
	out[1] = 0.0;
	out[2] = 0.0;
	double total_mass = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		for (int k = 0; k < 3; k++)
			out[k] +=
			    sim->bodies[i].mass * sim->bodies[i].position[k];
		total_mass += sim->bodies[i].mass;
	}
	for (int k = 0; k < 3; k++)
		out[k] /= total_mass;
}

static float max_body_distance(const NBodySim* sim)
{
	double max_dist = 0.0;
	for (int i = 0; i < sim->body_count; i++) {
		double dist = vec3_length(sim->bodies[i].position);
		if (dist > max_dist)
			max_dist = dist;
	}
	return (float)max_dist;
}

static void distances_from_star(const NBodySim* sim, double* out)
{
	for (int i = 1; i < sim->body_count; i++) {
		double diff[3] = {
		    sim->bodies[i].position[0] - sim->bodies[0].position[0],
		    sim->bodies[i].position[1] - sim->bodies[0].position[1],
		    sim->bodies[i].position[2] - sim->bodies[0].position[2]};
		out[i - 1] = vec3_length(diff);
	}
}

void test_nbody_long_run_stability(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	const int initial_count = sim.body_count;
	const float initial_energy = nbody_total_energy(&sim);

	double com_initial[3], dist_initial[NBODY_MAX_BODIES],
	    dist_peak[NBODY_MAX_BODIES];
	compute_center_of_mass(&sim, com_initial);
	distances_from_star(&sim, dist_initial);
	for (int i = 0; i < initial_count - 1; i++)
		dist_peak[i] = dist_initial[i];

	float sim_time = 0.0F;

	while (sim_time < SIM_DURATION) {
		nbody_step(&sim, STEP_DT);
		sim_time += STEP_DT;

		double dist_cur[NBODY_MAX_BODIES];
		distances_from_star(&sim, dist_cur);
		for (int i = 0; i < initial_count - 1; i++) {
			if (dist_cur[i] > dist_peak[i])
				dist_peak[i] = dist_cur[i];
		}
	}

	float energy_drift = fabsf(nbody_total_energy(&sim) - initial_energy) /
	                     (fabsf(initial_energy) + 1e-10F);
	double com_final[3];
	compute_center_of_mass(&sim, com_final);
	double com_shift[3] = {com_final[0] - com_initial[0],
	                       com_final[1] - com_initial[1],
	                       com_final[2] - com_initial[2]};

	TEST_ASSERT_EQUAL_INT_MESSAGE(initial_count, sim.body_count,
	                              "Body count changed");
	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(MAX_ENERGY_DRIFT, energy_drift,
	                                    "Energy drifted beyond threshold");
	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    MAX_COM_DRIFT, vec3_length(com_shift), "Center of mass drifted");
	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    MAX_BODY_DISTANCE, max_body_distance(&sim), "A body escaped");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_nbody_long_run_stability);
	return UNITY_END();
}
