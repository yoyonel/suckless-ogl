#include "trail_renderer.h"

#include "gl_common.h"
#include "log.h"
#include "nbody_types.h"
#include "platform/platform_utils.h"
#include "profiler.h"
#include "shader.h"
#include "utils.h"
#include <cglm/types.h>
#include <cglm/vec3.h>
#include <math.h>
#include <string.h>

/* Maximum ribbon vertices: each body can produce up to
 * (TRAIL_MAX_POINTS - 1) segments × 2 vertices + 2 degenerate = ~514.
 * With NBODY_MAX_BODIES=32: 32 × 514 = 16448 vertices.
 * Each vertex is 32 bytes → ~256 KB. Trivially small. */
enum { MAX_TRAIL_VERTICES = NBODY_MAX_BODIES * ((TRAIL_MAX_POINTS * 2) + 4) };

static const float EPSILON = 1e-6F;
static const float MIN_BODY_RADIUS = 0.3F;
static const float HALF = 0.5F;

/* ---------------------------------------------------------------------------
 * Ring buffer helpers
 * ---------------------------------------------------------------------------*/

static void ring_push(TrailRing* ring, const double pos[3], float timestamp)
{
	ring->head = (ring->head + 1) % TRAIL_MAX_POINTS;
	ring->points[ring->head][0] = (float)pos[0];
	ring->points[ring->head][1] = (float)pos[1];
	ring->points[ring->head][2] = (float)pos[2];
	ring->timestamps[ring->head] = timestamp;
	if (ring->count < TRAIL_MAX_POINTS) {
		ring->count++;
	}
}

static void ring_get(const TrailRing* ring, int age, vec3 out)
{
	int idx = (ring->head - age + TRAIL_MAX_POINTS) % TRAIL_MAX_POINTS;
	out[0] = ring->points[idx][0];
	out[1] = ring->points[idx][1];
	out[2] = ring->points[idx][2];
}

static float ring_get_timestamp(const TrailRing* ring, int age)
{
	int idx = (ring->head - age + TRAIL_MAX_POINTS) % TRAIL_MAX_POINTS;
	return ring->timestamps[idx];
}

/* ---------------------------------------------------------------------------
 * Init / Cleanup
 * ---------------------------------------------------------------------------*/

bool trail_renderer_init(TrailRenderer* trail, int body_count)
{
	(void)safe_memset(trail, sizeof(*trail), 0, sizeof(*trail));
	trail->body_count = body_count;

	// 1. Allocation dynamique alignée (16 ou 32 octets pour SIMD)
	size_t staging_size = MAX_TRAIL_VERTICES * sizeof(TrailVertex);
	trail->staging =
	    (TrailVertex*)platform_aligned_alloc(staging_size, SIMD_ALIGNMENT);
	if (!trail->staging) {
		LOG_ERROR("suckless-ogl.trail",
		          "Failed to allocate staging buffer");
		return false;
	}

	/* Load trail shader */
	trail->shader = shader_load("shaders/trail.vert", "shaders/trail.frag");
	if (!trail->shader) {
		LOG_ERROR("suckless-ogl.trail", "Failed to load trail shader");
		return false;
	}

	/* Create VAO + dynamic VBO */
	glGenVertexArrays(1, &trail->vao);
	glGenBuffers(1, &trail->vbo);

	glBindVertexArray(trail->vao);
	glBindBuffer(GL_ARRAY_BUFFER, trail->vbo);

	glObjectLabel(GL_VERTEX_ARRAY, trail->vao, -1, "Trail_VAO");
	glObjectLabel(GL_BUFFER, trail->vbo, -1, "Trail_VBO");
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(MAX_TRAIL_VERTICES * sizeof(TrailVertex)),
	             NULL, GL_STREAM_DRAW);

	/* Attribute 0: position (vec3) + u (float) — packed as vec4 */
	const GLsizei stride = (GLsizei)sizeof(TrailVertex);
	glEnableVertexAttribArray(0);
	glVertexAttribFormat(0, 4, GL_FLOAT, GL_FALSE,
	                     (GLuint)offsetof(TrailVertex, position));
	glVertexAttribBinding(0, 0);

	/* Attribute 1: color (vec3) + v (float) — packed as vec4 */
	glEnableVertexAttribArray(1);
	glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE,
	                     (GLuint)offsetof(TrailVertex, color));
	glVertexAttribBinding(1, 0);

	glBindVertexBuffer(0, trail->vbo, 0, stride);

	glBindVertexArray(0);

	/* Initialize neon glow defaults */
	trail->neon.intensity = TRAIL_NEON_INTENSITY_DEFAULT;
	trail->neon.core_exp = TRAIL_NEON_CORE_EXP_DEFAULT;
	trail->neon.width = TRAIL_NEON_WIDTH_DEFAULT;

	/* Initialize time-based trail parameters */
	trail->trail_duration = TRAIL_DURATION_DEFAULT;
	trail->sim_time = 0.0F;

	return true;
}

void trail_renderer_set_color(TrailRenderer* trail, int body_index,
                              const vec3 color)
{
	if (body_index >= 0 && body_index < NBODY_MAX_BODIES) {
		glm_vec3_copy((float*)color, trail->colors[body_index]);
	}
}

void trail_renderer_cleanup(TrailRenderer* trail)
{
	// 2. Libération propre
	if (trail->staging) {
		platform_aligned_free(trail->staging);
		trail->staging = NULL;
	}

	GL_SAFE_DELETE_BUFFER(trail->vbo);
	GL_SAFE_DELETE_VAO(trail->vao);
	if (trail->shader) {
		shader_destroy(trail->shader);
		trail->shader = NULL;
	}
}

void trail_renderer_clear(TrailRenderer* trail)
{
	for (int i = 0; i < trail->body_count; i++) {
		trail->rings[i].head = 0;
		trail->rings[i].count = 0;
	}
	trail->sample_timer = 0.0F;
}

/* ---------------------------------------------------------------------------
 * Record positions (rate-limited)
 * ---------------------------------------------------------------------------*/

void trail_renderer_record(TrailRenderer* trail, const NBodySim* sim,
                           float delta_time)
{
	/* Advance simulation time by the effective (time-scaled) delta.
	 * fabsf() ensures time reversal still advances the sampling clock —
	 * we always accumulate positive time for trail recording. */
	float effective_dt = delta_time * fabsf(sim->time_scale);
	trail->sim_time += effective_dt;
	trail->sample_timer += effective_dt;

	/* Emit as many sub-samples as needed (catches large time_scale
	 * jumps that skip multiple intervals in a single frame).
	 * Each sample is stamped with the current sim_time so the ribbon
	 * builder can compute age-based tapering independent of FPS. */
	while (trail->sample_timer >= TRAIL_SAMPLE_INTERVAL) {
		trail->sample_timer -= TRAIL_SAMPLE_INTERVAL;
		for (int i = 0; i < sim->body_count && i < trail->body_count;
		     i++) {
			ring_push(&trail->rings[i], sim->bodies[i].position,
			          trail->sim_time);
		}
	}
}

/* ---------------------------------------------------------------------------
 * Ribbon geometry builder
 *
 * For each body's trail, generate a camera-facing triangle strip:
 * - Two vertices per trail point, offset perpendicular to both the
 *   trail direction and the camera-to-point vector.
 * - Width tapers from TRAIL_MAX_WIDTH at head to 0 at tail.
 * - UV coordinates: U = age (0=head, 1=tail), V = side (0 or 1).
 * ---------------------------------------------------------------------------*/

static int build_ribbon(const TrailRing* ring, const vec3 color,
                        const vec3 cam_pos, float body_radius,
                        float hdr_intensity, float max_width,
                        float current_time, float trail_duration,
                        TrailVertex* out)
{
	if (ring->count < 3) {
		return 0;
	}

	int vcount = 0;
	const int num_pts = ring->count;

	for (int idx = 0; idx < num_pts - 1; idx++) {
		vec3 pos_cur;
		vec3 pos_next;
		ring_get(ring, idx, pos_cur);
		ring_get(ring, idx + 1, pos_next);

		/* Compute age from timestamp — independent of FPS */
		float sample_time = ring_get_timestamp(ring, idx);
		float age = (current_time - sample_time) / trail_duration;
		if (age < 0.0F) {
			age = 0.0F;
		}
		if (age > 1.0F) {
			continue; /* Too old — skip this point */
		}

		/* Trail segment direction */
		vec3 seg_dir;
		glm_vec3_sub(pos_next, pos_cur, seg_dir);
		float seg_len = glm_vec3_norm(seg_dir);
		if (seg_len < EPSILON) {
			continue;
		}
		glm_vec3_scale(seg_dir, 1.0F / seg_len, seg_dir);

		/* Camera-facing perpendicular (billboard) */
		vec3 to_cam;
		glm_vec3_sub((float*)cam_pos, pos_cur, to_cam);
		glm_vec3_normalize(to_cam);

		vec3 side;
		glm_vec3_cross(seg_dir, to_cam, side);
		float side_len = glm_vec3_norm(side);
		if (side_len < EPSILON) {
			/* Segment points directly at camera — use fallback */
			vec3 up_dir = {0.0F, 1.0F, 0.0F};
			glm_vec3_cross(seg_dir, up_dir, side);
			side_len = glm_vec3_norm(side);
			if (side_len < EPSILON) {
				vec3 right_dir = {1.0F, 0.0F, 0.0F};
				glm_vec3_cross(seg_dir, right_dir, side);
				side_len = glm_vec3_norm(side);
			}
		}
		if (side_len > EPSILON) {
			glm_vec3_scale(side, 1.0F / side_len, side);
		}

		/* Width: cubic taper, scaled by body radius for
		 * proportional trails. Minimum radius clamp. */
		float base_width =
		    max_width * fmaxf(body_radius, MIN_BODY_RADIUS);
		float width = base_width * (1.0F - (age * age * age));

		/* HDR color with quadratic intensity fade */
		float intensity = (1.0F - age) * (1.0F - age);
		vec3 hdr_color;
		glm_vec3_scale((float*)color, hdr_intensity * intensity,
		               hdr_color);

		/* Left vertex (v=0) */
		TrailVertex* vtx_left = &out[vcount++];
		glm_vec3_add(
		    pos_cur,
		    (vec3){side[0] * width, side[1] * width, side[2] * width},
		    vtx_left->position);
		vtx_left->u = age;
		glm_vec3_copy(hdr_color, vtx_left->color);
		vtx_left->v = 0.0F;

		/* Right vertex (v=1) */
		TrailVertex* vtx_right = &out[vcount++];
		glm_vec3_sub(
		    pos_cur,
		    (vec3){side[0] * width, side[1] * width, side[2] * width},
		    vtx_right->position);
		vtx_right->u = age;
		glm_vec3_copy(hdr_color, vtx_right->color);
		vtx_right->v = 1.0F;
	}

	return vcount;
}

/* ---------------------------------------------------------------------------
 * Draw
 * ---------------------------------------------------------------------------*/

void trail_renderer_draw(TrailRenderer* trail, mat4 view, mat4 proj,
                         vec3 cam_pos)
{
	// 3. Utilisation du buffer de la structure plutôt que le static
	TrailVertex* staging = trail->staging;
	int total_verts = 0;

	/* Track per-body start offsets and vertex counts for batched draw.
	 * glMultiDrawArrays draws N independent strips in one call. */
	GLint body_start[NBODY_MAX_BODIES];
	GLsizei body_count_v[NBODY_MAX_BODIES];
	int active_bodies = 0;

	{
		PROFILE_ZONE(ribbon_ctx, "Trail Ribbon Build");
		for (int i = 0; i < trail->body_count; i++) {
			int start = total_verts;
			int vcount = build_ribbon(
			    &trail->rings[i], trail->colors[i], cam_pos,
			    1.0F, /* body_radius — could be passed in */
			    trail->neon.intensity, trail->neon.width,
			    trail->sim_time, trail->trail_duration,
			    &staging[total_verts]);
			if (vcount > 0) {
				body_start[active_bodies] = start;
				body_count_v[active_bodies] = vcount;
				active_bodies++;
				total_verts += vcount;
			}
		}
		PROFILE_ZONE_END(ribbon_ctx);
	}

	if (total_verts == 0) {
		return;
	}

	/* Upload ribbon geometry to GPU */
	{
		PROFILE_ZONE(upload_ctx, "Trail VBO Upload");
		glBindBuffer(GL_ARRAY_BUFFER, trail->vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0,
		                (GLsizeiptr)(total_verts * sizeof(TrailVertex)),
		                staging);
		PROFILE_ZONE_END(upload_ctx);
	}

	/* Set rendering state */
	shader_use(trail->shader);
	shader_set_mat4(trail->shader, "u_view", (const float*)view);
	shader_set_mat4(trail->shader, "u_proj", (const float*)proj);
	shader_set_float(trail->shader, "u_core_exp", trail->neon.core_exp);

	glBindVertexArray(trail->vao);

	/* Additive blending — trails glow and accumulate */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	/* Read depth but don't write — trails are transparent overlay */
	glDepthMask(GL_FALSE);

	/* Disable writing to the velocity buffer (Attachment 1) to prevent
	 * motion blur artifacts */
	glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	/* Draw all body trails in a single batched call.
	 * glMultiDrawArrays draws N independent triangle strips without
	 * degenerate vertices or per-strip driver overhead. */
	{
		PROFILE_ZONE(draw_ctx, "Trail Draw Calls");
		glMultiDrawArrays(GL_TRIANGLE_STRIP, body_start, body_count_v,
		                  active_bodies);
		PROFILE_ZONE_END(draw_ctx);
	}

	/* Restore state */
	glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glBindVertexArray(0);
}

/* ---------------------------------------------------------------------------
 * Duration accessors
 * ---------------------------------------------------------------------------*/

float trail_renderer_get_duration(const TrailRenderer* trail)
{
	return trail->trail_duration;
}

void trail_renderer_set_duration(TrailRenderer* trail, float duration)
{
	if (duration < TRAIL_DURATION_MIN) {
		duration = TRAIL_DURATION_MIN;
	}
	if (duration > TRAIL_DURATION_MAX) {
		duration = TRAIL_DURATION_MAX;
	}
	trail->trail_duration = duration;
}
