#ifndef SCENE_LIGHTING_H
#define SCENE_LIGHTING_H

/**
 * @file scene_lighting.h
 * @brief IBL, probes, and materials sub-struct extracted from Scene.
 */

#include "ibl_coordinator.h"
#include "light_probes.h"
#include "material.h"

/**
 * @struct SceneLighting
 * @brief IBL, probes, and materials grouped to reduce Scene fan-out.
 */
typedef struct SceneLighting {
	MaterialLib* material_lib; /**< Loaded material presets. */
	IBLCoordinator ibl_coord;  /**< IBL state machine. */
	LightProbeGrid probe_grid; /**< Global Illumination spatial grid. */
} SceneLighting;

#endif /* SCENE_LIGHTING_H */
