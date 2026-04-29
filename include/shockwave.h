/**
 * @file shockwave.h
 * @brief Visual shockwave effect for N-body confinement boundary impacts.
 *
 * When a body crosses the confinement radius, a radial energy ring
 * expands from the impact point and fades out.  Rendered as
 * screen-aligned quads that sample the scene behind them and apply
 * radial UV distortion with chromatic aberration — a localized
 * lens-like refraction effect.
 */

#ifndef SHOCKWAVE_H
#define SHOCKWAVE_H

#include "gl_common.h"
#include <cglm/types.h>
#include <stdbool.h>

typedef struct Shader Shader;

/** Maximum simultaneous active shockwaves. */
enum { SHOCKWAVE_MAX_ACTIVE = 16 };

/** Shockwave lifetime in seconds. */
static const float SHOCKWAVE_DURATION = 1.2F * 3.0F;

/** Maximum ring expansion radius in world units. */
static const float SHOCKWAVE_MAX_RADIUS = 6.0F;

/** Minimum velocity to trigger a shockwave (avoids spam from bodies
 *  barely grazing the boundary). */
static const float SHOCKWAVE_MIN_VELOCITY = 0.05F;

/** HDR intensity for the shockwave ring (drives bloom). */
static const float SHOCKWAVE_HDR_INTENSITY = 4.0F;

/**
 * @struct ShockwaveEvent
 * @brief One active shockwave expanding from an impact point.
 */
typedef struct {
	vec3 position;    /**< World-space impact position. */
	float start_time; /**< Simulation time when the impact occurred. */
	vec3 color;       /**< HDR color (derived from body albedo). */
	float intensity;  /**< Based on impact velocity (0-1 range). */
} ShockwaveEvent;

/**
 * @struct ShockwaveRenderer
 * @brief Manages and renders active shockwave effects.
 */
typedef struct {
	ShockwaveEvent events[SHOCKWAVE_MAX_ACTIVE]; /**< Ring buffer. */
	int count; /**< Number of active events. */

	Shader* shader; /**< Shockwave shader program. */
	GLuint vao;     /**< Quad VAO. */
	GLuint vbo;     /**< Quad VBO (4 vertices). */

	/* Grab pass: copy of scene color before shockwave draw so the
	 * fragment shader can read scene pixels without read/write hazard. */
	GLuint grab_tex;        /**< Copy of scene color (RGBA16F). */
	int grab_width;         /**< Allocated grab texture width. */
	int grab_height;        /**< Allocated grab texture height. */
	GLuint scene_color_tex; /**< Handle to live scene FBO color (set
	                           externally). */
} ShockwaveRenderer;

/**
 * @brief Initializes shockwave renderer (shader + GPU resources).
 * @return true on success, false on shader load failure.
 */
bool shockwave_renderer_init(ShockwaveRenderer* renderer);

/**
 * @brief Releases all GPU resources.
 */
void shockwave_renderer_cleanup(ShockwaveRenderer* renderer);

/**
 * @brief Registers a new shockwave at the given position.
 *
 * Oldest event is evicted if the buffer is full.
 *
 * @param sw       Renderer state.
 * @param position World-space impact point.
 * @param color    Body albedo color.
 * @param velocity Radial velocity at impact (determines intensity).
 * @param sim_time Current simulation time.
 */
void shockwave_emit(ShockwaveRenderer* renderer, const vec3 position,
                    const vec3 color, float velocity, float sim_time);

/**
 * @brief Removes expired shockwaves based on current simulation time.
 */
void shockwave_update(ShockwaveRenderer* renderer, float sim_time);

/**
 * @brief Draws all active shockwaves as billboard quads with lensing.
 *
 * Performs a grab pass (copies scene_color_tex to an internal texture)
 * then draws billboard quads that sample the copy and apply radial
 * UV distortion with chromatic aberration.
 *
 * Should be called after scene geometry, before post-processing.
 *
 * @param renderer   Renderer state.
 * @param view       View matrix.
 * @param proj       Projection matrix.
 * @param camera_pos Camera world position.
 * @param sim_time   Current simulation time.
 * @param screen_w   Viewport width in pixels.
 * @param screen_h   Viewport height in pixels.
 */
void shockwave_draw(const ShockwaveRenderer* renderer, mat4 view, mat4 proj,
                    vec3 camera_pos, float sim_time, int screen_w,
                    int screen_h);

#endif /* SHOCKWAVE_H */
