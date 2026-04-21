/**
 * @file nbody.h
 * @brief N-body gravitational simulation (Velocity Verlet + Plummer softening).
 *
 * Manages a set of gravitational bodies with position, velocity, and mass.
 * Uses a symplectic Velocity Verlet integrator with per-pair Plummer
 * softening to guarantee long-term energy conservation without ad-hoc
 * damping.  Produces SphereInstance data for the instanced renderer.
 *
 * @see docs/nbody_physics.md for the full physics reference.
 */

#ifndef NBODY_H
#define NBODY_H

#include "instanced_rendering.h"
#include <cglm/types.h>
#include <stdbool.h>

/** Maximum number of bodies in the simulation. */
enum { NBODY_MAX_BODIES = 16 };

/** Default gravitational constant (tuned for visual scale ~20 units). */
static const float NBODY_DEFAULT_G = 1.0F;

/** Minimum softening factor squared — prevents singularity at r→0.
 *  Actual softening per pair is max(this, (F*(r_i + r_j))²) so that bodies
 *  repel naturally before visually overlapping (Plummer softening). */
static const float NBODY_SOFTENING_SQ = 0.25F;

/** Per-pair softening multiplier.
 *  eps = F * (r_i + r_j).  Higher values weaken close encounters
 *  and reduce chaotic 3-body slingshot effects at the cost of
 *  making gravity less Keplerian at short range. */
static const float NBODY_SOFTENING_FACTOR = 2.0F;

/** Fixed physics timestep for stable integration (seconds). */
static const float NBODY_FIXED_DT = 1.0F / 120.0F;

/** Maximum accumulated physics time per call to nbody_step.
 *  Prevents runaway integration after lag spikes or first frame. */
static const float NBODY_MAX_ACCUMULATOR = 1.0F / 30.0F;

/**
 * @struct NBodyParticle
 * @brief A single gravitational body.
 */
typedef struct {
	vec3 position;      /**< World position. */
	float mass;         /**< Gravitational mass. */
	vec3 velocity;      /**< Current velocity. */
	float radius;       /**< Visual sphere radius (for rendering scale). */
	vec3 albedo;        /**< PBR base color for this body. */
	float metallic;     /**< PBR metallic factor. */
	float roughness;    /**< PBR roughness factor. */
	vec3 prev_position; /**< Position from previous frame (motion blur). */
} NBodyParticle;

/**
 * @struct NBodySim
 * @brief State container for the N-body simulation.
 */
typedef struct {
	NBodyParticle bodies[NBODY_MAX_BODIES]; /**< Array of bodies. */
	int body_count;                         /**< Number of active bodies. */
	float gravity;        /**< Gravitational constant G. */
	float accumulator;    /**< Physics timestep accumulator. */
	float time_scale;     /**< Speed multiplier (1.0 = real-time). */
	float initial_energy; /**< Total energy at init (E₀ reference). */
	bool paused;          /**< If true, simulation does not advance. */
} NBodySim;

/**
 * @brief Initializes the simulation with a preset solar system configuration.
 * @param sim Pointer to the simulation state.
 */
void nbody_init_preset(NBodySim* sim);

/**
 * @brief Advances the simulation by the given wall-clock delta time.
 *
 * Uses a fixed-timestep accumulator with Velocity Verlet integration.
 * @param sim Pointer to the simulation state.
 * @param delta_time Wall-clock time elapsed since last call (seconds).
 */
void nbody_step(NBodySim* sim, float delta_time);

/**
 * @brief Writes current body positions and materials into a SphereInstance
 *        array suitable for the instanced rendering pipeline.
 * @param sim Pointer to the simulation state.
 * @param out Output array (must have capacity >= sim->body_count).
 */
void nbody_write_instances(const NBodySim* sim, SphereInstance* out);

/**
 * @brief Returns the number of active bodies.
 */
int nbody_get_count(const NBodySim* sim);

/**
 * @brief Computes the total energy (kinetic + gravitational potential).
 *
 * Useful for stability diagnostics.  With a symplectic integrator,
 * this quantity oscillates with bounded amplitude around E₀.
 */
float nbody_total_energy(const NBodySim* sim);

/**
 * @brief Computes kinetic energy only: Σ ½ m v².
 */
float nbody_kinetic_energy(const NBodySim* sim);

/**
 * @brief Returns the relative energy drift |E(t) - E₀| / |E₀|.
 *
 * Returns 0 when initial_energy is zero (no reference).
 */
float nbody_energy_drift(const NBodySim* sim);

#endif /* NBODY_H */
