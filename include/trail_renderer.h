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

/** Maximum trail sample points per body. */
enum { TRAIL_MAX_POINTS = 256 };

/** HDR intensity multiplier — drives bloom for neon glow effect. */
static const float TRAIL_HDR_INTENSITY = 5.0F;

/** Maximum ribbon half-width in world units. */
static const float TRAIL_MAX_WIDTH = 0.24F;

/** Minimum time between trail samples (seconds). */
static const float TRAIL_SAMPLE_INTERVAL = 1.0F / 60.0F;

/** Default trail duration in seconds (how long a trail persists). */
static const float TRAIL_DURATION_DEFAULT = 4.0F;

/** Minimum / maximum trail duration for runtime adjustment. */
static const float TRAIL_DURATION_MIN = 0.5F;
static const float TRAIL_DURATION_MAX = 30.0F;
static const float TRAIL_DURATION_STEP = 0.5F;

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
	float timestamps[TRAIL_MAX_POINTS]; /**< Simulation time at each sample.
	                                     */
	int head;                           /**< Write index (newest). */
	int count;                          /**< Number of valid samples. */
} TrailRing;

/**
 * @enum TrailNeonParam
 * @brief Identifies which neon parameter is being adjusted.
 */
typedef enum {
	TRAIL_NEON_PARAM_INTENSITY = 0,
	TRAIL_NEON_PARAM_CORE,
	TRAIL_NEON_PARAM_WIDTH,
	TRAIL_NEON_PARAM_COUNT
} TrailNeonParam;

/**
 * @struct TrailNeonParams
 * @brief Runtime-adjustable neon glow profile parameters.
 */
typedef struct {
	float intensity; /**< HDR intensity multiplier (default 5.0). */
	float core_exp;  /**< Core tightness exponent (default 12.0). */
	float width;     /**< Ribbon half-width multiplier (default 0.24). */
	TrailNeonParam active; /**< Currently selected param for adjustment. */
} TrailNeonParams;

/** Default neon parameter values. */
static const float TRAIL_NEON_INTENSITY_DEFAULT = 5.0F;
static const float TRAIL_NEON_CORE_EXP_DEFAULT = 12.0F;
static const float TRAIL_NEON_WIDTH_DEFAULT = 0.24F;
static const float TRAIL_NEON_INTENSITY_STEP = 0.5F;
static const float TRAIL_NEON_CORE_STEP = 2.0F;
static const float TRAIL_NEON_WIDTH_STEP = 0.02F;
static const float TRAIL_NEON_INTENSITY_MIN = 0.5F;
static const float TRAIL_NEON_CORE_MIN = 2.0F;
static const float TRAIL_NEON_WIDTH_MIN = 0.04F;

/**
 * @struct TrailRenderer
 * @brief Manages trail state and GPU resources for all bodies.
 */
typedef struct {
	TrailRing rings[NBODY_MAX_BODIES]; /**< Per-body position history. */
	int body_count;                    /**< Number of tracked bodies. */
	float sample_timer;                /**< Accumulator for sample rate. */
	float sim_time;                    /**< Monotonic simulation time. */
	float trail_duration;              /**< Trail lifetime in seconds. */

	/* GPU Resources */
	GLuint vao;       /**< VAO for ribbon geometry. */
	GLuint vbo;       /**< Dynamic VBO for ribbon vertices. */
	Shader* shader;   /**< Trail rendering shader program. */
	int vertex_count; /**< Vertices written this frame. */

	/* Per-body trail colors (HDR emissive). */
	vec3 colors[NBODY_MAX_BODIES];

	/* Runtime neon glow parameters */
	TrailNeonParams neon; /**< Adjustable neon profile. */
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
 * @brief Returns the current trail duration in seconds.
 */
float trail_renderer_get_duration(const TrailRenderer* tr);

/**
 * @brief Sets the trail duration in seconds (clamped to valid range).
 */
void trail_renderer_set_duration(TrailRenderer* tr, float duration);

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
