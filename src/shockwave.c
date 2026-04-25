#include "shockwave.h"

#include "gl_common.h"
#include "log.h"
#include "shader.h"
#include "utils.h"
#include <cglm/vec3.h>

/* Billboard quad: 4 vertices, triangle strip, centered at origin.
 * Scaled per-shockwave to the current ring radius in the vertex shader. */
static const float QUAD_VERTICES[] = {
    -1.0F, -1.0F, 0.0F, /* bottom-left  */
    1.0F,  -1.0F, 0.0F, /* bottom-right */
    -1.0F, 1.0F,  0.0F, /* top-left     */
    1.0F,  1.0F,  0.0F, /* top-right    */
};

static const int QUAD_VERTEX_COUNT = 4;
static const int QUAD_FLOATS = 12;

/* ---------------------------------------------------------------------------
 * Init / Cleanup
 * ---------------------------------------------------------------------------*/

bool shockwave_renderer_init(ShockwaveRenderer* renderer)
{
	(void)safe_memset(renderer, sizeof(*renderer), 0, sizeof(*renderer));

	renderer->shader =
	    shader_load("shaders/shockwave.vert", "shaders/shockwave.frag");
	if (!renderer->shader) {
		LOG_ERROR("suckless-ogl.shockwave",
		          "Failed to load shockwave shader");
		return false;
	}

	/* Create VAO + static quad VBO */
	glGenVertexArrays(1, &renderer->vao);
	glGenBuffers(1, &renderer->vbo);
	glObjectLabel(GL_VERTEX_ARRAY, renderer->vao, -1, "Shockwave_VAO");
	glObjectLabel(GL_BUFFER, renderer->vbo, -1, "Shockwave_VBO");

	glBindVertexArray(renderer->vao);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(QUAD_FLOATS * sizeof(float)),
	             QUAD_VERTICES, GL_STATIC_DRAW);

	/* Attribute 0: position (vec3) */
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
	                      3 * (GLsizei)sizeof(float), NULL);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);

	return true;
}

void shockwave_renderer_cleanup(ShockwaveRenderer* renderer)
{
	GL_SAFE_DELETE_BUFFER(renderer->vbo);
	GL_SAFE_DELETE_VAO(renderer->vao);
	if (renderer->grab_tex) {
		glDeleteTextures(1, &renderer->grab_tex);
		renderer->grab_tex = 0;
	}
	if (renderer->shader) {
		shader_destroy(renderer->shader);
		renderer->shader = NULL;
	}
}

/* ---------------------------------------------------------------------------
 * Event management
 * ---------------------------------------------------------------------------*/

void shockwave_emit(ShockwaveRenderer* renderer, const vec3 position,
                    const vec3 color, float velocity, float sim_time)
{
	if (velocity < SHOCKWAVE_MIN_VELOCITY) {
		return;
	}

	/* Evict oldest if full */
	int idx = renderer->count;
	if (idx >= SHOCKWAVE_MAX_ACTIVE) {
		/* Find the oldest event (earliest start_time) and replace */
		idx = 0;
		for (int i = 1; i < SHOCKWAVE_MAX_ACTIVE; i++) {
			if (renderer->events[i].start_time <
			    renderer->events[idx].start_time) {
				idx = i;
			}
		}
	} else {
		renderer->count++;
	}

	glm_vec3_copy((float*)position, renderer->events[idx].position);
	glm_vec3_copy((float*)color, renderer->events[idx].color);
	renderer->events[idx].start_time = sim_time;

	/* Normalise intensity: clamp velocity to [0, 1] range */
	static const float MAX_IMPACT_VELOCITY = 10.0F;
	float norm = velocity / MAX_IMPACT_VELOCITY;
	if (norm > 1.0F) {
		norm = 1.0F;
	}
	renderer->events[idx].intensity = norm;
}

void shockwave_update(ShockwaveRenderer* renderer, float sim_time)
{
	int write = 0;
	for (int read = 0; read < renderer->count; read++) {
		float age = sim_time - renderer->events[read].start_time;
		if (age < SHOCKWAVE_DURATION) {
			if (write != read) {
				renderer->events[write] =
				    renderer->events[read];
			}
			write++;
		}
	}
	renderer->count = write;
}

/* ---------------------------------------------------------------------------
 * Grab pass: ensure grab_tex matches screen dimensions, then copy.
 * ---------------------------------------------------------------------------*/

static void ensure_grab_texture(ShockwaveRenderer* ren, int width, int height)
{
	if (ren->grab_tex && ren->grab_width == width &&
	    ren->grab_height == height) {
		return; /* already the right size */
	}

	if (!ren->grab_tex) {
		glGenTextures(1, &ren->grab_tex);
		glObjectLabel(GL_TEXTURE, ren->grab_tex, -1,
		              "Shockwave_GrabTex");
	}
	glBindTexture(GL_TEXTURE_2D, ren->grab_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA,
	             GL_HALF_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	ren->grab_width = width;
	ren->grab_height = height;
}

/* ---------------------------------------------------------------------------
 * Rendering
 * ---------------------------------------------------------------------------*/

void shockwave_draw(const ShockwaveRenderer* renderer, mat4 view, mat4 proj,
                    vec3 camera_pos, float sim_time, int screen_w, int screen_h)
{
	if (renderer->count == 0 || !renderer->shader) {
		return;
	}

	/* --- Grab pass: copy current scene color into grab_tex --- */
	/* Cast away const: we only mutate the lazily-allocated grab texture,
	 * which is an internal rendering resource, not logical state. */
	ensure_grab_texture((ShockwaveRenderer*)renderer, screen_w, screen_h);

	/* Copy from currently bound READ framebuffer (scene_fbo) */
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, renderer->grab_tex);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, screen_w, screen_h);

	/* --- Draw billboard quads with lensing shader --- */
	shader_use(renderer->shader);
	shader_set_mat4(renderer->shader, "u_view", (float*)view);
	shader_set_mat4(renderer->shader, "u_proj", (float*)proj);
	shader_set_vec3(renderer->shader, "u_camera_pos", camera_pos);

	/* Bind grab texture on unit 0 for the fragment shader */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->grab_tex);
	shader_set_int(renderer->shader, "u_grab_tex", 0);

	/* Alpha blending: the distorted scene replaces the original pixels.
	 * SRC_ALPHA / ONE_MINUS_SRC_ALPHA so the ring area shows distorted
	 * scene and the outer fringe blends to the original. */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	glBindVertexArray(renderer->vao);

	for (int i = 0; i < renderer->count; i++) {
		const ShockwaveEvent* evt = &renderer->events[i];
		float age = sim_time - evt->start_time;
		float progress = age / SHOCKWAVE_DURATION; /* 0..1 */

		/* Ring expands over time */
		float radius = SHOCKWAVE_MAX_RADIUS * progress;

		shader_set_vec3(renderer->shader, "u_center", evt->position);
		shader_set_vec3(renderer->shader, "u_color", evt->color);
		shader_set_float(renderer->shader, "u_radius", radius);
		shader_set_float(renderer->shader, "u_progress", progress);
		shader_set_float(renderer->shader, "u_intensity",
		                 evt->intensity);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, QUAD_VERTEX_COUNT);
	}

	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}
