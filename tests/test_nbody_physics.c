/**
 * @file test_nbody_physics.c
 * @brief Tests unitaires du cœur mathématique N-Body (Velocity Verlet).
 */

#include "nbody.h"
#include "unity.h"
#include <math.h>
#include <stdio.h>

static const float STEP_DT = 1.0F / 60.0F;
static const float LAG_SPIKE_DT = 0.5F;
static const float SECONDARY_SPIKE_DT = 0.2F;
static const float MAX_BODY_DISTANCE = 50.0F;

enum {
	PAUSE_TEST_STEPS = 1000,
	SPIKE_NORMAL_STEPS = 3600,
	ZERO_GRAV_STEPS = 120,
};

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

void test_nbody_single_step_sanity(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);
	nbody_step(&sim, STEP_DT);

	for (int i = 0; i < sim.body_count; i++) {
		for (int k = 0; k < 3; k++) {
			TEST_ASSERT_FALSE_MESSAGE(
			    isnan(sim.bodies[i].position[k]),
			    "NaN in body position");
			TEST_ASSERT_FALSE_MESSAGE(
			    isnan(sim.bodies[i].velocity[k]),
			    "NaN in body velocity");
			TEST_ASSERT_FALSE_MESSAGE(
			    isinf(sim.bodies[i].position[k]),
			    "Inf in body position");
			TEST_ASSERT_FALSE_MESSAGE(
			    isinf(sim.bodies[i].velocity[k]),
			    "Inf in body velocity");
		}
	}
}

void test_nbody_zero_gravity(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);
	sim.gravity = 0.0F;

	double vel_before[3] = {sim.bodies[1].velocity[0],
	                        sim.bodies[1].velocity[1],
	                        sim.bodies[1].velocity[2]};

	for (int i = 0; i < ZERO_GRAV_STEPS; i++) {
		nbody_step(&sim, STEP_DT);
	}

	TEST_ASSERT_FLOAT_WITHIN(1e-4F, vel_before[0],
	                         sim.bodies[1].velocity[0]);
	TEST_ASSERT_FLOAT_WITHIN(1e-4F, vel_before[1],
	                         sim.bodies[1].velocity[1]);
	TEST_ASSERT_FLOAT_WITHIN(1e-4F, vel_before[2],
	                         sim.bodies[1].velocity[2]);
}

void test_nbody_paused_no_change(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	double pos_before[3] = {sim.bodies[1].position[0],
	                        sim.bodies[1].position[1],
	                        sim.bodies[1].position[2]};

	sim.paused = true;
	for (int i = 0; i < PAUSE_TEST_STEPS; i++) {
		nbody_step(&sim, STEP_DT);
	}

	TEST_ASSERT_EQUAL_DOUBLE(pos_before[0], sim.bodies[1].position[0]);
	TEST_ASSERT_EQUAL_DOUBLE(pos_before[1], sim.bodies[1].position[1]);
	TEST_ASSERT_EQUAL_DOUBLE(pos_before[2], sim.bodies[1].position[2]);
}

void test_nbody_survives_dt_spikes(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	nbody_step(&sim, LAG_SPIKE_DT);
	nbody_step(&sim, SECONDARY_SPIKE_DT);
	for (int i = 0; i < SPIKE_NORMAL_STEPS; i++) {
		nbody_step(&sim, STEP_DT);
	}

	double dist_star[NBODY_MAX_BODIES];
	distances_from_star(&sim, dist_star);

	for (int i = 0; i < sim.body_count - 1; i++) {
		TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
		    MAX_BODY_DISTANCE, dist_star[i],
		    "Body escaped after dt spike");
	}
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_nbody_single_step_sanity);
	RUN_TEST(test_nbody_zero_gravity);
	RUN_TEST(test_nbody_paused_no_change);
	RUN_TEST(test_nbody_survives_dt_spikes);
	return UNITY_END();
}
