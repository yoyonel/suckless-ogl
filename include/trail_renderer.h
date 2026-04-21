/**
 * @file trail_renderer.h
 * @brief GPU-accelerated ribbon trail renderer for N-body simulation.
 *
 * Records positions over time for each body and renders camera-facing
 * ribbon geometry with:
 * - Width tapering (thick at head, zero at tail)
 * - Soft anti-aliased edges via fragment shader smoothstep
 * - HDR emissive colors for bloom integration
 * - Additive blending for glow accumulation
 */

#ifndef TRAIL_RENDERER_H
#define TRAIL_RENDERER_H

#include "gl_common.h"
#include "nbody.h"
#include "shader.h"
#include <cglm/types.h>
#include <stdbool.h>

/** Maximum trail sample points per body. At 60 samples/sec ≈ 4.3 seconds. */
enum { TRAIL_MAX_POINTS = 256 };

/** HDR intensity multiplier — ensures bloom threshold is exceeded. */
static const float TRAIL_HDR_INTENSITY = 3.0F;

/** Maximum ribbon half-width in world units. */
static const float TRAIL_MAX_WIDTH = 0.18F;

/** Minimum time between trail samples (seconds). */
static const float TRAIL_SAMPLE_INTERVAL = 1.0F / 60.0F;

/**
 * @struct TrailVertex
 * @brief Per-vertex data for the ribbon triangle strip.
 *
 * 32 bytes per vertex, cache-line friendly.
 */
typedef struct {
	float position[3]; /**< World position. */
	float u;           /**< Along trail: 0=head, 1=tail. */
	float color[3];    /**< HDR emissive color (pre-multiplied). */
	float v;           /**< Across ribbon: 0=left edge, 1=right edge. */
} TrailVertex;

/**
 * @struct TrailRing
 * @brief Ring buffer of recorded positions for one body.
 */
typedef struct {
	vec3 points[TRAIL_MAX_POINTS]; /**< Circular buffer of positions. */
	int head;                      /**< Write index (newest). */
	int count;                     /**< Number of valid samples. */
} TrailRing;

/**
 * @struct TrailRenderer
 * @brief Manages trail state and GPU resources for all bodies.
 */
typedef struct {
	TrailRing rings[NBODY_MAX_BODIES]; /**< Per-body position history. */
	int body_count;                    /**< Number of tracked bodies. */
	float sample_timer;                /**< Accumulator for sample rate. */

	/* GPU Resources */
	GLuint vao;       /**< VAO for ribbon geometry. */
	GLuint vbo;       /**< Dynamic VBO for ribbon vertices. */
	Shader* shader;   /**< Trail rendering shader program. */
	int vertex_count; /**< Vertices written this frame. */

	/* Per-body trail colors (HDR emissive). */
	vec3 colors[NBODY_MAX_BODIES];
} TrailRenderer;

/**
 * @brief Initializes trail renderer resources.
 * @param tr Pointer to the trail renderer.
 * @param body_count Number of bodies to track.
 * @return true on success, false on failure.
 */
bool trail_renderer_init(TrailRenderer* tr, int body_count);

/**
 * @brief Sets the trail color for a specific body.
 * @param tr Pointer to the trail renderer.
 * @param body_index Body index.
 * @param color HDR emissive color (values > 1.0 are valid for bloom).
 */
void trail_renderer_set_color(TrailRenderer* tr, int body_index,
                              const vec3 color);

/**
 * @brief Records current positions from the simulation.
 *
 * Should be called every frame. Internally rate-limits sampling.
 * @param tr Pointer to the trail renderer.
 * @param sim Pointer to the N-body simulation.
 * @param delta_time Frame delta time.
 */
void trail_renderer_record(TrailRenderer* tr, const NBodySim* sim,
                           float delta_time);

/**
 * @brief Builds ribbon geometry and renders all trails.
 *
 * Must be called while the HDR framebuffer is active (before post-process).
 * Sets up additive blending and depth-read-no-write internally.
 * @param tr Pointer to the trail renderer.
 * @param view View matrix.
 * @param proj Projection matrix.
 * @param cam_pos Camera world position.
 */
void trail_renderer_draw(TrailRenderer* tr, mat4 view, mat4 proj, vec3 cam_pos);

/**
 * @brief Clears all recorded trail data (e.g., on simulation reset).
 * @param tr Pointer to the trail renderer.
 */
void trail_renderer_clear(TrailRenderer* tr);

/**
 * @brief Releases all GPU resources.
 * @param tr Pointer to the trail renderer.
 */
void trail_renderer_cleanup(TrailRenderer* tr);

#endif /* TRAIL_RENDERER_H */
