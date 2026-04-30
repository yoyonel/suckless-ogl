#ifndef SCENE_SHADERS_H
#define SCENE_SHADERS_H

/**
 * @file scene_shaders.h
 * @brief Shader pointers and uniform caches extracted from Scene.
 */

#include "scene_uniforms.h"
#include "shader.h"

/**
 * @struct SceneShaders
 * @brief All shader pointers and uniform caches owned by the scene.
 */
typedef struct SceneShaders {
	Shader* pbr_instanced; /**< Shared PBR shader for opaque geo. */
	Shader* pbr_billboard; /**< Shader for volumetric/alpha spheres. */
	Shader* debug;         /**< Generic debug/visualization shader. */
	Shader* debug_line;    /**< Shader for wireframe lines. */
	Shader* skybox;        /**< Skybox shader wrapper. */
#ifdef USE_SSBO_RENDERING
	Shader* pbr_ssbo; /**< Optimized SSBO shader. */
#endif

	/* --- Uniform Caches --- */
	BillboardUniforms billboard_uniforms; /**< Cached locations. */
	InstancedUniforms instanced_uniforms; /**< Cached locations. */
	DebugUniforms debug_uniforms;         /**< Cached locations. */
} SceneShaders;

#endif /* SCENE_SHADERS_H */
