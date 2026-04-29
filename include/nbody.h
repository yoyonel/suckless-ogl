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

#include "sphere_types.h"
#include <cglm/types.h>
#include <stdbool.h>

/** Maximum number of bodies in the simulation. */
enum { NBODY_MAX_BODIES = 32 };

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
 *  Prevents runaway integration after lag spikes or first frame.
 *  Set to 1/10s to support frame rates as low as ~10 FPS without
 *  losing simulation time (12 Verlet steps per frame for N=14). */
static const float NBODY_MAX_ACCUMULATOR = 1.0F / 10.0F;

/** Confinement radius — bodies beyond this distance from the central star
 *  experience a quadratic restoring potential V = ½k(r - r_max)².
 *  The potential is conservative (Hamiltonian) so symplecticity is preserved.
 */
static const float NBODY_CONFINEMENT_RADIUS = 25.0F;

/** Confinement stiffness — spring constant for the restoring force.
 *  Higher values = harder wall, lower = softer bounce.
 *  k·dt² ≈ 5·(1/120)² ≈ 0.0003 — well within stability limit. */
static const float NBODY_CONFINEMENT_K = 5.0F;

/** Radial damping coefficient in the confinement zone.
 *  Effective damping is γ · (overshoot / r_max), so the base value
 *  must be large enough that bodies near the boundary still feel it.
 *  At r = 26 (overshoot 1): γ_eff = 20 × 1/25 = 0.8
 *  At r = 30 (overshoot 5): γ_eff = 20 × 5/25 = 4.0
 *  Only the radial (v·r̂) component is damped; tangential speed is
 *  preserved so angular momentum is not destroyed. */
static const float NBODY_CONFINEMENT_DAMPING = 20.0F;

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
/** Rate at which time_scale transitions toward its target (units/s). */
static const float NBODY_TIME_SCALE_RATE = 3.0F;

/**
 * @struct NBodyImpact
 * @brief Records a confinement boundary hit for visual effects.
 *
 * One slot per body (indexed by body index).  Across the multiple
 * integration sub-steps of a single frame, only the strongest
 * outward velocity is kept — this naturally deduplicates.
 */
typedef struct {
	vec3 position;  /**< World position of impact. */
	vec3 color;     /**< Body albedo at impact. */
	float velocity; /**< Peak outward radial speed this frame. */
	bool active;    /**< True if body hit the boundary this frame. */
} NBodyImpact;

typedef struct NBodySim {
	NBodyParticle bodies[NBODY_MAX_BODIES]; /**< Array of bodies. */
	int body_count;                         /**< Number of active bodies. */
	float gravity;           /**< Gravitational constant G. */
	float accumulator;       /**< Physics timestep accumulator. */
	float time_scale;        /**< Speed multiplier (1.0 = real-time). */
	float target_time_scale; /**< Smooth-transition target. */
	float initial_energy;    /**< Total energy at init (E₀ reference). */
	float sim_time;          /**< Monotonic simulation clock (seconds). */
	bool paused;             /**< If true, simulation does not advance. */

	/* Confinement impact events — one slot per body, cleared each frame.
	 * Only the peak velocity across integration sub-steps is kept. */
	NBodyImpact impacts[NBODY_MAX_BODIES]; /**< Per-body impacts. */
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

/**
 * Signed energy drift: negative = energy lost (damping),
 * positive = energy gained (divergence).
 */
float nbody_energy_drift_signed(const NBodySim* sim);

/**
 * @brief Smoothly transitions time_scale toward target_time_scale.
 *
 * Called once per frame.  Ramps at NBODY_TIME_SCALE_RATE units/s,
 * producing a decelerate → pause → accelerate-in-reverse effect.
 * @param sim Pointer to the simulation state.
 * @param delta_time Wall-clock frame delta (seconds).
 */
void nbody_update_time_scale(NBodySim* sim, float delta_time);

#endif /* NBODY_H */
