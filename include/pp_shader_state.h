#ifndef PP_SHADER_STATE_H
#define PP_SHADER_STATE_H

/**
 * @file pp_shader_state.h
 * @brief Shader management state for the optimized uber-shader pipeline.
 */

#include <stdbool.h>

typedef struct Shader Shader;

/**
 * @struct ShaderCacheEntry
 * @brief Cache entry for optimized shaders.
 */
typedef struct {
	unsigned int flags;
	Shader* shader;
} ShaderCacheEntry;

enum { SHADER_CACHE_SIZE = 64 };

/**
 * @struct PPShaderState
 * @brief Shader management state for the optimized uber-shader pipeline.
 */
typedef struct {
	Shader* postprocess_shader;  /**< Main Uber-shader. */
	Shader* tile_max_shader;     /**< Motion blur helper. */
	Shader* neighbor_max_shader; /**< Motion blur helper. */
	bool is_optimized; /**< true if Uber-shader uses static preprocessor
	                      flags. */
	unsigned int
	    compiled_flags; /**< Flags used for the current optimized shader. */
	ShaderCacheEntry shader_cache[SHADER_CACHE_SIZE];
	int shader_cache_count;
} PPShaderState;

#endif /* PP_SHADER_STATE_H */
