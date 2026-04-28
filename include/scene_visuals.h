#ifndef SCENE_VISUALS_H
#define SCENE_VISUALS_H

/**
 * @file scene_visuals.h
 * @brief Visual effects sub-struct extracted from Scene.
 */

#include "shockwave.h"
#include "skybox.h"
#include "trail_renderer.h"

/**
 * @struct SceneVisuals
 * @brief Visual effects grouped into a sub-struct to reduce Scene fan-out.
 */
typedef struct SceneVisuals {
	Skybox skybox;                        /**< Environment renderer. */
	TrailRenderer trail_renderer;         /**< Orbital trail renderer. */
	ShockwaveRenderer shockwave_renderer; /**< Confinement impact VFX. */
} SceneVisuals;

#endif /* SCENE_VISUALS_H */
