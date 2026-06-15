/**
 * @file nbody.h
 * @brief N-body gravitational simulation (Velocity Verlet + Plummer softening).
 *
 * Manages a set of gravitational bodies with position, velocity, and mass.
 * Uses a symplectic Velocity Verlet integrator with per-pair Plummer
 * softening to guarantee long-term energy conservation without ad-hoc
 * damping. Produces SphereInstance data for the instanced renderer.
 *
 * @see docs/nbody_physics.md for the full physics reference.
 */

#ifndef NBODY_H
#define NBODY_H

#include "nbody_types.h"
#include "sphere_types.h" /* Apporte la définition de SphereInstance */

/* --- Initialisation --- */
/**
 * @brief Initializes the simulation with a preset solar system configuration.
 */
void nbody_init_preset(NBodySim* sim);

/* --- Boucle Principale (Run) --- */
/**
 * @brief Advances the simulation by the given wall-clock delta time.
 */
void nbody_step(NBodySim* sim, float delta_time);

/**
 * @brief Smoothly transitions time_scale toward target_time_scale.
 */
void nbody_update_time_scale(NBodySim* sim, float delta_time);

/* --- Rendu --- */
/**
 * @brief Writes current body positions and materials into a SphereInstance
 * array.
 */
void nbody_write_instances(const NBodySim* sim, SphereInstance* out);

/**
 * @brief Returns the number of active bodies.
 */
int nbody_get_count(const NBodySim* sim);

/* --- Diagnostiques / Debug --- */
/**
 * @brief Computes the total energy (kinetic + gravitational potential).
 */
float nbody_total_energy(const NBodySim* sim);

/**
 * @brief Computes kinetic energy only: Σ ½ m v².
 */
float nbody_kinetic_energy(const NBodySim* sim);

/**
 * @brief Returns the relative energy drift |E(t) - E₀| / |E₀|.
 */
float nbody_energy_drift(const NBodySim* sim);

/**
 * @brief Signed energy drift.
 */
float nbody_energy_drift_signed(const NBodySim* sim);

#endif /* NBODY_H */
