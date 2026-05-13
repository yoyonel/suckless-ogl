#ifndef SCENE_SIMULATION_H
#define SCENE_SIMULATION_H

/**
 * @file scene_simulation.h
 * @brief N-body simulation sub-struct extracted from Scene.
 */

#include "nbody.h"

/**
 * @struct SceneSimulation
 * @brief N-body simulation state grouped to reduce Scene fan-out.
 */
typedef struct SceneSimulation {
	NBodySim nbody_sim; /**< N-body gravitational simulation. */
	int nbody_mode;     /**< Toggle: 0=grid, 1=N-body. */
} SceneSimulation;

#endif /* SCENE_SIMULATION_H */
