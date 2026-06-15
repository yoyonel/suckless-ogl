#ifndef NBODY_TYPES_H
#define NBODY_TYPES_H

#include <cglm/types.h>
#include <stdbool.h>

/** Maximum number of bodies in the simulation. */
enum { NBODY_MAX_BODIES = 32 };

/** Default gravitational constant (tuned for visual scale ~20 units). */
static const float NBODY_DEFAULT_G = 1.0F;

/** Minimum softening factor squared — prevents singularity at r→0. */
static const float NBODY_SOFTENING_SQ = 0.25F;

/** Per-pair softening multiplier. */
static const float NBODY_SOFTENING_FACTOR = 2.0F;

/** Fixed physics timestep for stable integration (seconds). */
static const float NBODY_FIXED_DT = 1.0F / 120.0F;

/** Maximum accumulated physics time per call to nbody_step. */
static const float NBODY_MAX_ACCUMULATOR = 1.0F / 10.0F;

/** Confinement radius */
static const float NBODY_CONFINEMENT_RADIUS = 25.0F;

/** Confinement stiffness */
static const float NBODY_CONFINEMENT_K = 5.0F;

/** Radial damping coefficient in the confinement zone. */
static const float NBODY_CONFINEMENT_DAMPING = 20.0F;

/** Rate at which time_scale transitions toward its target (units/s). */
static const float NBODY_TIME_SCALE_RATE = 3.0F;

/**
 * @struct NBodyParticle
 * @brief A single gravitational body.
 */
typedef struct {
	double position[3]; /**< World position. */
	double mass;        /**< Gravitational mass. */
	double velocity[3]; /**< Current velocity. */
	float radius;       /**< Visual sphere radius (for rendering scale). */
	vec3 albedo;        /**< PBR base color for this body. */
	float metallic;     /**< PBR metallic factor. */
	float roughness;    /**< PBR roughness factor. */
	double prev_position[3]; /**< Position from previous frame (motion
	                            blur). */
} NBodyParticle;

/**
 * @struct NBodyImpact
 * @brief Records a confinement boundary hit for visual effects.
 */
typedef struct {
	vec3 position;  /**< World position of impact. */
	vec3 color;     /**< Body albedo at impact. */
	float velocity; /**< Peak outward radial speed this frame. */
	bool active;    /**< True if body hit the boundary this frame. */
} NBodyImpact;

/**
 * @struct NBodySim
 * @brief State container for the N-body simulation.
 */
typedef struct NBodySim {
	NBodyParticle bodies[NBODY_MAX_BODIES]; /**< Array of bodies. */
	int body_count;                         /**< Number of active bodies. */
	float gravity;           /**< Gravitational constant G. */
	float accumulator;       /**< Physics timestep accumulator. */
	float time_scale;        /**< Speed multiplier. */
	float target_time_scale; /**< Smooth-transition target. */
	float initial_energy;    /**< Total energy at init. */
	float sim_time;          /**< Monotonic simulation clock. */
	bool paused;             /**< Simulation state. */
	NBodyImpact impacts[NBODY_MAX_BODIES]; /**< Per-body impacts. */
} NBodySim;

#endif /* NBODY_TYPES_H */
