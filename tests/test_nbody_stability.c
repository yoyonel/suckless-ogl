/**
 * @file test_nbody_stability.c
 * @brief N-body physics stability tests — pure CPU, no GPU required.
 *
 * Runs the simulation for long periods (hundreds of simulated seconds)
 * at maximum speed and verifies invariants:
 * - Total energy stays bounded (symplectic integrator conservation).
 * - Center of mass does not drift.
 * - No body escapes to infinity.
 * - Body count remains constant.
 */

#include "nbody.h"
#include "unity.h"
#include <math.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Test parameters
 * ---------------------------------------------------------------------------*/

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

/** Simulated time for the long-run test (seconds). */
static const float SIM_DURATION = 1200.0F;

/** Maximum allowed energy drift ratio: |E - E0| / |E0|. */
static const float MAX_ENERGY_DRIFT = 0.05F;

/** Maximum allowed center-of-mass drift (absolute distance). */
static const float MAX_COM_DRIFT = 0.5F;

/** Maximum allowed distance of any body from origin. */
static const float MAX_BODY_DISTANCE = 50.0F;

/** Wall-clock delta fed to nbody_step each iteration. */
static const float STEP_DT = 1.0F / 60.0F;

/** Progress report interval in simulated seconds. */
static const float REPORT_INTERVAL = 60.0F;

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

void setUp(void)
{
}

void tearDown(void)
{
}

/* ---------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------------*/

static void compute_center_of_mass(const NBodySim* sim, float out[3])
{
	out[0] = 0.0F;
	out[1] = 0.0F;
	out[2] = 0.0F;
	float total_mass = 0.0F;
	for (int i = 0; i < sim->body_count; i++) {
		for (int k = 0; k < 3; k++) {
			out[k] +=
			    sim->bodies[i].mass * sim->bodies[i].position[k];
		}
		total_mass += sim->bodies[i].mass;
	}
	for (int k = 0; k < 3; k++) {
		out[k] /= total_mass;
	}
}

static float vec3_length(const float v[3])
{
	return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static float max_body_distance(const NBodySim* sim)
{
	float max_dist = 0.0F;
	for (int i = 0; i < sim->body_count; i++) {
		float dist = vec3_length(sim->bodies[i].position);
		if (dist > max_dist) {
			max_dist = dist;
		}
	}
	return max_dist;
}

/** Distance of each satellite (body 1..N) from the central star (body 0). */
static void distances_from_star(const NBodySim* sim, float* out)
{
	for (int i = 1; i < sim->body_count; i++) {
		float diff[3] = {
		    sim->bodies[i].position[0] - sim->bodies[0].position[0],
		    sim->bodies[i].position[1] - sim->bodies[0].position[1],
		    sim->bodies[i].position[2] - sim->bodies[0].position[2]};
		out[i - 1] = vec3_length(diff);
	}
}

/* ---------------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------------*/

/**
 * Long-run stability: simulate 1200s of physics and check invariants.
 */
void test_nbody_long_run_stability(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	const int initial_count = sim.body_count;
	const float initial_energy = nbody_total_energy(&sim);

	float com_initial[3];
	compute_center_of_mass(&sim, com_initial);

	/* Record initial distances from star for each satellite. */
	float dist_initial[NBODY_MAX_BODIES];
	distances_from_star(&sim, dist_initial);

	/* Track peak distance per body across the entire run. */
	float dist_peak[NBODY_MAX_BODIES];
	for (int i = 0; i < initial_count - 1; i++) {
		dist_peak[i] = dist_initial[i];
	}

	float sim_time = 0.0F;
	float next_report = REPORT_INTERVAL;

	printf(
	    "\n  [nbody stability] E0 = %.4f, bodies = %d, "
	    "simulating %.0fs...\n",
	    (double)initial_energy, initial_count, (double)SIM_DURATION);

	while (sim_time < SIM_DURATION) {
		nbody_step(&sim, STEP_DT);
		sim_time += STEP_DT;

		/* Update peak distances from star. */
		float dist_cur[NBODY_MAX_BODIES];
		distances_from_star(&sim, dist_cur);
		for (int i = 0; i < initial_count - 1; i++) {
			if (dist_cur[i] > dist_peak[i]) {
				dist_peak[i] = dist_cur[i];
			}
		}

		if (sim_time >= next_report) {
			float energy = nbody_total_energy(&sim);
			float drift = fabsf(energy - initial_energy) /
			              (fabsf(initial_energy) + 1e-10F);
			float com[3];
			compute_center_of_mass(&sim, com);
			float com_dist = vec3_length(com);

			printf(
			    "  [%6.0fs] E=%.4f (drift=%.4f%%) "
			    "CoM=%.4f  d_star=",
			    (double)sim_time, (double)energy,
			    (double)(drift * 100.0F), (double)com_dist);
			for (int i = 0; i < initial_count - 1; i++) {
				printf("%.1f ", (double)dist_cur[i]);
			}
			printf("\n");

			next_report += REPORT_INTERVAL;
		}
	}

	/* --- Assertions --- */
	float final_energy = nbody_total_energy(&sim);
	float energy_drift = fabsf(final_energy - initial_energy) /
	                     (fabsf(initial_energy) + 1e-10F);

	float com_final[3];
	compute_center_of_mass(&sim, com_final);
	float com_shift[3] = {com_final[0] - com_initial[0],
	                      com_final[1] - com_initial[1],
	                      com_final[2] - com_initial[2]};
	float com_drift = vec3_length(com_shift);

	float farthest = max_body_distance(&sim);

	/* Per-body distances from star at the end. */
	float dist_final[NBODY_MAX_BODIES];
	distances_from_star(&sim, dist_final);

	printf(
	    "  [FINAL] E_drift=%.4f%%  CoM_drift=%.4f  "
	    "max_r=%.2f  bodies=%d\n",
	    (double)(energy_drift * 100.0F), (double)com_drift,
	    (double)farthest, sim.body_count);

	printf("  [DISTANCES from star]  initial → peak → final\n");
	for (int i = 0; i < initial_count - 1; i++) {
		printf("    body %d: %6.2f → %6.2f → %6.2f\n", i + 1,
		       (double)dist_initial[i], (double)dist_peak[i],
		       (double)dist_final[i]);
	}

	TEST_ASSERT_EQUAL_INT_MESSAGE(initial_count, sim.body_count,
	                              "Body count changed during simulation");

	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    MAX_ENERGY_DRIFT, energy_drift,
	    "Energy drifted beyond acceptable threshold");

	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    MAX_COM_DRIFT, com_drift,
	    "Center of mass drifted beyond acceptable threshold");

	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    MAX_BODY_DISTANCE, farthest,
	    "A body escaped beyond maximum allowed distance");

	/* Per-body: peak distance should not exceed MAX_BODY_DISTANCE. */
	for (int i = 0; i < initial_count - 1; i++) {
		char msg[80];  // NOLINT(readability-magic-numbers)
		(void)snprintf(msg, sizeof(msg),
		               "Body %d peak distance %.1f exceeds limit",
		               i + 1, (double)dist_peak[i]);
		TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(MAX_BODY_DISTANCE,
		                                    dist_peak[i], msg);
	}
}

/**
 * Quick check: single step does not crash or produce NaN.
 */
void test_nbody_single_step_sanity(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	nbody_step(&sim, STEP_DT);

	for (int i = 0; i < sim.body_count; i++) {
		for (int k = 0; k < 3; k++) {
			TEST_ASSERT_FALSE_MESSAGE(
			    isnan(sim.bodies[i].position[k]),
			    "NaN in body position after single step");
			TEST_ASSERT_FALSE_MESSAGE(
			    isnan(sim.bodies[i].velocity[k]),
			    "NaN in body velocity after single step");
			TEST_ASSERT_FALSE_MESSAGE(
			    isinf(sim.bodies[i].position[k]),
			    "Inf in body position after single step");
			TEST_ASSERT_FALSE_MESSAGE(
			    isinf(sim.bodies[i].velocity[k]),
			    "Inf in body velocity after single step");
		}
	}
}

/**
 * Verify symplectic energy conservation: even after a velocity
 * perturbation, the integrator keeps energy bounded over time
 * (no secular growth). With no energy clamp, this validates that
 * the pure Hamiltonian integrator handles perturbations gracefully.
 */
void test_nbody_energy_conservation(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	float energy_before = nbody_total_energy(&sim);

	/* Boost body 1 velocity to inject energy */
	// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	sim.bodies[1].velocity[0] += 10.0F;
	sim.bodies[1].velocity[1] += 10.0F;
	// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	float energy_boosted = nbody_total_energy(&sim);
	TEST_ASSERT_GREATER_THAN_FLOAT(energy_before, energy_boosted);

	/* Run 120 steps — energy should remain near the boosted level
	 * (symplectic: bounded oscillation, no secular drift). */
	// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	for (int i = 0; i < 120; i++) {
		nbody_step(&sim, STEP_DT);
	}
	// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	float energy_after = nbody_total_energy(&sim);
	printf("  [conservation] E0=%.2f  boosted=%.2f  after=%.2f\n",
	       (double)energy_before, (double)energy_boosted,
	       (double)energy_after);

	/* Energy should stay close to the boosted level (new E0 of the
	 * perturbed system). Drift from boosted should be small. */
	float drift = fabsf(energy_after - energy_boosted) /
	              (fabsf(energy_boosted) + 1e-10F);
	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    0.20F, drift, "Energy drifted significantly from perturbed level");
}

/**
 * Paused simulation should not change state.
 */
void test_nbody_paused_no_change(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	float pos_before[3] = {sim.bodies[1].position[0],
	                       sim.bodies[1].position[1],
	                       sim.bodies[1].position[2]};

	sim.paused = true;
	// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	for (int i = 0; i < 1000; i++) {
		nbody_step(&sim, STEP_DT);
	}
	// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	TEST_ASSERT_EQUAL_FLOAT(pos_before[0], sim.bodies[1].position[0]);
	TEST_ASSERT_EQUAL_FLOAT(pos_before[1], sim.bodies[1].position[1]);
	TEST_ASSERT_EQUAL_FLOAT(pos_before[2], sim.bodies[1].position[2]);
}

/**
 * Simulate what happens in the real app: large dt spikes (first frame,
 * window resize, lag). Before the accumulator cap, these caused runaway
 * integration that ejected light bodies.
 */
void test_nbody_survives_dt_spikes(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	/* Simulate a realistic app session:
	 * - 1 huge spike (first frame: 0.5s)
	 * - then normal frames at 60fps for 60s */
	// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	nbody_step(&sim, 0.5F); /* Lag spike */
	nbody_step(&sim, 0.2F); /* Another spike */
	for (int i = 0; i < 3600; i++) {
		nbody_step(&sim, STEP_DT);
	}
	// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	float dist_star[NBODY_MAX_BODIES];
	distances_from_star(&sim, dist_star);

	printf(
	    "  [spike] after 0.5s+0.2s spikes + 60s normal: "
	    "d_star=");
	for (int i = 0; i < sim.body_count - 1; i++) {
		printf("%.1f ", (double)dist_star[i]);
	}
	printf("\n");

	/* No body should have escaped. */
	for (int i = 0; i < sim.body_count - 1; i++) {
		TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
		    MAX_BODY_DISTANCE, dist_star[i],
		    "Body escaped after dt spike");
	}
}

/**
 * Verify nbody_kinetic_energy returns a positive value after init.
 */
void test_nbody_kinetic_energy_positive(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	float kin = nbody_kinetic_energy(&sim);
	TEST_ASSERT_GREATER_THAN_FLOAT(0.0F, kin);
	printf("  [kinetic] Ek = %.4f J\n", (double)kin);
}

/**
 * Verify initial_energy is stored and energy_drift starts near zero.
 */
void test_nbody_energy_drift_api(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);

	/* E0 should be stored and non-zero */
	TEST_ASSERT_NOT_EQUAL_FLOAT(0.0F, sim.initial_energy);

	/* Drift at t=0 should be exactly 0 */
	float drift = nbody_energy_drift(&sim);
	TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.0F, drift);

	/* After a few steps, drift should stay small (< 5%) */
	// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	for (int i = 0; i < 600; i++) {
		nbody_step(&sim, STEP_DT);
	}
	// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	drift = nbody_energy_drift(&sim);
	printf("  [drift API] after 10s: drift = %.4f%%\n",
	       (double)(drift * 100.0F));
	TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(
	    MAX_ENERGY_DRIFT, drift,
	    "Energy drift API exceeded threshold after 10s");
}

/**
 * Verify gravity=0 produces zero gravitational acceleration
 * (bodies move in straight lines).
 */
void test_nbody_zero_gravity(void)
{
	NBodySim sim;
	nbody_init_preset(&sim);
	sim.gravity = 0.0F;

	/* Record velocity of body 1 */
	float vel_before[3] = {sim.bodies[1].velocity[0],
	                       sim.bodies[1].velocity[1],
	                       sim.bodies[1].velocity[2]};

	// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	for (int i = 0; i < 120; i++) {
		nbody_step(&sim, STEP_DT);
	}
	// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	/* With G=0, velocity should be unchanged (no forces). */
	TEST_ASSERT_FLOAT_WITHIN(1e-4F, vel_before[0],
	                         sim.bodies[1].velocity[0]);
	TEST_ASSERT_FLOAT_WITHIN(1e-4F, vel_before[1],
	                         sim.bodies[1].velocity[1]);
	TEST_ASSERT_FLOAT_WITHIN(1e-4F, vel_before[2],
	                         sim.bodies[1].velocity[2]);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_nbody_single_step_sanity);
	RUN_TEST(test_nbody_energy_conservation);
	RUN_TEST(test_nbody_paused_no_change);
	RUN_TEST(test_nbody_survives_dt_spikes);
	RUN_TEST(test_nbody_kinetic_energy_positive);
	RUN_TEST(test_nbody_energy_drift_api);
	RUN_TEST(test_nbody_zero_gravity);
	RUN_TEST(test_nbody_long_run_stability);
	return UNITY_END();
}
