/**
 * @file test_nbody_diagnostics.c
 * @brief Tests unitaires des mesures d'énergie et de la conservation
 * symplectique.
 */

#include "nbody.h"
#include "unity.h"
#include <math.h>
#include <stdio.h>

static const float STEP_DT = 1.0F / 60.0F;
static const float VELOCITY_BOOST = 10.0F;
static const float CONSERVATION_MAX_DRIFT = 0.65F;
static const float MAX_ENERGY_DRIFT = 0.06F;

enum {
	CONSERVATION_STEPS = 120,
	DRIFT_API_STEPS = 600,
};

void setUp(void)
{
}

void tearDown(void)
{
}

void test_nbody_kinetic_energy_positive(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);
	TEST_ASSERT_GREATER_THAN_FLOAT(0.0F, nbody_kinetic_energy(&sim));
}

void test_nbody_energy_drift_api(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	TEST_ASSERT_NOT_EQUAL_FLOAT(0.0F, sim.initial_energy);
	TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.0F, nbody_energy_drift(&sim));

	for (int i = 0; i < DRIFT_API_STEPS; i++) {
		nbody_step(&sim, STEP_DT);
	}

	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    MAX_ENERGY_DRIFT, nbody_energy_drift(&sim),
	    "Drift API exceeded threshold after 10s");
}

void test_nbody_energy_conservation(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	float energy_before = nbody_total_energy(&sim);
	sim.bodies[1].velocity[0] += VELOCITY_BOOST;
	sim.bodies[1].velocity[1] += VELOCITY_BOOST;

	float energy_boosted = nbody_total_energy(&sim);
	TEST_ASSERT_GREATER_THAN_FLOAT(energy_before, energy_boosted);

	for (int i = 0; i < CONSERVATION_STEPS; i++) {
		nbody_step(&sim, STEP_DT);
	}

	float energy_after = nbody_total_energy(&sim);
	float drift = fabsf(energy_after - energy_boosted) /
	              (fabsf(energy_boosted) + 1e-10F);
	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    CONSERVATION_MAX_DRIFT, drift,
	    "Energy drifted from perturbed level");
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_nbody_kinetic_energy_positive);
	RUN_TEST(test_nbody_energy_drift_api);
	RUN_TEST(test_nbody_energy_conservation);
	return UNITY_END();
}
