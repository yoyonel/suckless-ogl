#ifndef SCENE_INTERNAL_H
#define SCENE_INTERNAL_H

/**
 * @file scene_internal.h
 * @brief Internal declarations shared across scene_*.c translation units.
 *
 * NOT part of the public API — do not include from outside src/scene_*.c.
 */

#include "scene.h"

/**
 * @brief Initialize instanced rendering data from the material library.
 *
 * Builds the SphereInstance array, uploads it to VBO, and configures
 * billboard + instanced groups. Also seeds the light probe grid.
 *
 * Used by scene_init (via scene_init_instanced_shader) and scene_toggle_nbody.
 */
void scene_init_instancing(Scene* scene);

#endif /* SCENE_INTERNAL_H */
