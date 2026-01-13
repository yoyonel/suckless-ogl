#include "app.h"

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/vec3.h>
#include <cglm/util.h>
#include <cglm/types.h>

#include <math.h>
#include <stdio.h>

#include "icosphere.h"
#include "shader.h"
#include "skybox.h"
#include "texture.h"

#include "log.h"

#define MOUSE_SENSITIVITY 0.002F
#define MIN_PITCH -1.5F
#define MAX_PITCH 1.5F

enum {
	MIN_SUBDIV = 0,
	MAX_SUBDIV = 6,
	CUBEMAP_SIZE = 1024,
	INITIAL_SUBDIVISIONS = 2
};

static const float DEFAULT_CAMERA_DISTANCE = 3.0F;
static const float DEFAULT_ENV_LOD = 0.0F;
static const float DEFAULT_CAMERA_ANGLE = 0.0F;
static const float NEAR_PLANE = 0.1F;
static const float FAR_PLANE = 100.0F;
static const float FOV_ANGLE = 60.0F;
static const float MAX_ENV_LOD = 10.0F;
static const float MIN_ENV_LOD = 0.0F;
static const float LOD_STEP = 0.5F;
static const float MIN_CAMERA_DISTANCE = 1.5F;
static const float MAX_CAMERA_DISTANCE = 10.0F;
static const float ZOOM_STEP = 0.2F;
static const float LIGHT_DIR_X = 0.5F;
static const float LIGHT_DIR_Y = 1.0F;
static const float LIGHT_DIR_Z = 0.3F;

static void key_callback(GLFWwindow* window, int key, int scancode, int action,
                         int mods);
static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static void framebuffer_size_callback(GLFWwindow* window, int width,
                                      int height);
static void app_toggle_fullscreen(App* app, GLFWwindow* window);

int app_init(App* app, int width, int height, const char* title)
{
	app->width = width;
	app->height = height;
	app->subdivisions = INITIAL_SUBDIVISIONS;
	app->wireframe = 0;

	/* Camera initial state */
	app->camera_yaw = DEFAULT_CAMERA_ANGLE;
	app->camera_pitch = DEFAULT_CAMERA_ANGLE;
	app->camera_distance = DEFAULT_CAMERA_DISTANCE;
	app->camera_enabled = 1; /* Enabled by default */
	app->env_lod = DEFAULT_ENV_LOD;     /* Default blur level */
	app->is_fullscreen = 0;
	app->first_mouse = 1;
	app->last_mouse_x = 0.0;
	app->last_mouse_y = 0.0;

	/* Initialize GLFW */
	if (!glfwInit()) {
		LOG_ERROR("suckless-ogl.app", "Failed to initialize GLFW");
		return 0;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	app->window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (!app->window) {
		LOG_ERROR("suckless-ogl.app", "Failed to create window");
		glfwTerminate();
		return 0;
	}

	glfwMakeContextCurrent(app->window);
	glfwSwapInterval(0); /* Disable VSync for performance comparison */
	glfwSetWindowUserPointer(app->window, app);
	glfwSetKeyCallback(app->window, key_callback);
	glfwSetCursorPosCallback(app->window, mouse_callback);
	glfwSetScrollCallback(app->window, scroll_callback);
	glfwSetFramebufferSizeCallback(app->window, framebuffer_size_callback);

	/* Enable mouse capture by default */
	if (app->camera_enabled) {
		glfwSetInputMode(app->window, GLFW_CURSOR,
		                 GLFW_CURSOR_DISABLED);
	}

	/* Initialize GLAD */
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		LOG_ERROR("suckless-ogl.app", "Failed to initialize GLAD");
		glfwDestroyWindow(app->window);
		glfwTerminate();
		return 0;
	}

	int major =
	    glfwGetWindowAttrib(app->window, GLFW_CONTEXT_VERSION_MAJOR);
	int minor =
	    glfwGetWindowAttrib(app->window, GLFW_CONTEXT_VERSION_MINOR);
	LOG_INFO("suckless-ogl.init", "Context Version: %d.%d", major, minor);
	LOG_INFO("suckless_ogl.context.base.window", "vendor: %s",
	         glGetString(GL_VENDOR));
	LOG_INFO("suckless_ogl.context.base.window", "renderer: %s",
	         glGetString(GL_RENDERER));
	LOG_INFO("suckless_ogl.context.base.window", "version: %s",
	         glGetString(GL_VERSION));
	LOG_INFO("suckless_ogl.context.base.window", "platform: linux");
	LOG_INFO("suckless_ogl.context.base.window", "code: 450");

	/* Load HDR texture */
	int hdr_w = 0;
	int hdr_h = 0;
	app->hdr_texture = texture_load_hdr("assets/env.hdr", &hdr_w, &hdr_h);
	if (!app->hdr_texture) {
		LOG_ERROR("suckless-ogl.app", "Failed to load HDR texture");
		return 0;
	}
	LOG_INFO("suckless-ogl.app",
	         "Asset Loading Time: (skipping exact timing for now)");

	/* Load shaders */
	app->phong_shader =
	    shader_load_program("shaders/phong.vert", "shaders/phong.frag");
	if (!app->phong_shader) {
		LOG_ERROR("suckless-ogl.app", "Failed to load phong shader");
		return 0;
	}

	app->skybox_shader = shader_load_program("shaders/background.vert",
	                                         "shaders/background.frag");

	if (!app->phong_shader || !app->skybox_shader) {
		LOG_ERROR("suckless-ogl.app", "Failed to create shaders");
		return 0;
	}

	/* Initialize skybox */
	skybox_init(&app->skybox, app->skybox_shader);

	/* Cache Phong shader uniforms */
	app->u_phong_mvp = glGetUniformLocation(app->phong_shader, "uMVP");
	app->u_phong_light_dir =
	    glGetUniformLocation(app->phong_shader, "lightDir");

	/* Initialize icosphere geometry */
	icosphere_init(&app->geometry);

	/* Create OpenGL buffers */
	glGenVertexArrays(1, &app->vao);
	glGenBuffers(1, &app->vbo);
	glGenBuffers(1, &app->nbo);
	glGenBuffers(1, &app->ebo);

	/* Enable depth testing */
	glEnable(GL_DEPTH_TEST);

	return 1;
}

void app_cleanup(App* app)
{
	icosphere_free(&app->geometry);
	skybox_cleanup(&app->skybox);

	glDeleteVertexArrays(1, &app->vao);
	glDeleteBuffers(1, &app->vbo);
	glDeleteBuffers(1, &app->nbo);
	glDeleteBuffers(1, &app->ebo);

	glDeleteTextures(1, &app->hdr_texture);
	glDeleteProgram(app->phong_shader);
	glDeleteProgram(app->skybox_shader);

	glfwDestroyWindow(app->window);
	glfwTerminate();
}

void app_run(App* app)
{
	int last_subdiv = -1;

	while (!glfwWindowShouldClose(app->window)) {
		/* Regenerate icosphere if subdivision level changed */
		if (app->subdivisions != last_subdiv) {
			icosphere_generate(&app->geometry, app->subdivisions);

			/* Upload to GPU */
			glBindVertexArray(app->vao);

			glBindBuffer(GL_ARRAY_BUFFER, app->vbo);
			glBufferData(GL_ARRAY_BUFFER,
			             (GLsizeiptr)(app->geometry.vertices.size * sizeof(vec3)),
			             app->geometry.vertices.data,
			             GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			                      sizeof(vec3), (void*)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, app->nbo);
			glBufferData(GL_ARRAY_BUFFER,
			             (GLsizeiptr)(app->geometry.normals.size * sizeof(vec3)),
			             app->geometry.normals.data,
			             GL_STATIC_DRAW);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
			                      sizeof(vec3), (void*)0);
			glEnableVertexAttribArray(1);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->ebo);
			glBufferData(
			    GL_ELEMENT_ARRAY_BUFFER,
			    (GLsizeiptr)(app->geometry.indices.size * sizeof(unsigned int)),
			    app->geometry.indices.data, GL_STATIC_DRAW);

			glBindVertexArray(0);
			last_subdiv = app->subdivisions;
		}

		app_render(app);

		glfwSwapBuffers(app->window);
		glfwPollEvents();
	}
}

void app_render(App* app)
{
	/* Explicitly clear color and depth.
	   Many Intel drivers perform better with color clear than without it.
	 */
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* Calculate camera position from yaw, pitch, and distance */
	float cam_x = app->camera_distance * cosf(app->camera_pitch) *
	              sinf(app->camera_yaw);
	float cam_y = app->camera_distance * sinf(app->camera_pitch);
	float cam_z = app->camera_distance * cosf(app->camera_pitch) *
	              cosf(app->camera_yaw);

	/* Setup camera matrices */
	mat4 view;
	mat4 proj;
	mat4 view_proj;
	mat4 inv_view_proj;
	vec3 camera_pos = {cam_x, cam_y, cam_z};
	vec3 target = {0.0F, 0.0F, 0.0F};
	vec3 camera_up = {0.0F, 1.0F, 0.0F};

	glm_lookat(camera_pos, target, camera_up, view);
	glm_perspective(glm_rad(FOV_ANGLE), (float)app->width / (float)app->height,
	                NEAR_PLANE, FAR_PLANE, proj);

	/* Calculate MVP for icosphere */
	glm_mat4_mul(proj, view, view_proj);

	/* For skybox: view matrix without translation */
	mat4 view_no_translation;
	glm_mat4_copy(view, view_no_translation);
	view_no_translation[3][0] = 0.0F; /* Remove X translation */
	view_no_translation[3][1] = 0.0F; /* Remove Y translation */
	view_no_translation[3][2] = 0.0F; /* Remove Z translation */

	/* Calculate inverse view-projection for skybox */
	glm_mat4_mul(proj, view_no_translation, inv_view_proj);
	glm_mat4_inv(inv_view_proj, inv_view_proj);

	/* 1. Render icosphere FIRST (populates depth buffer for early-Z
	 * culling) */
	app_render_icosphere(app, view_proj);

	/* 2. Render skybox LAST (using LEQUAL to fill background) */
	/* We always use FILL for the skybox regardless of wireframe mode */
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	skybox_render(&app->skybox, app->skybox_shader, app->hdr_texture,
	              inv_view_proj, app->env_lod);
}

void app_render_icosphere(App* app, mat4 view_proj)
{
	/* The icosphere is stationary at the origin */
	mat4 model;
	glm_mat4_identity(model);

	/* Build MVP matrix */
	mat4 mvp;
	glm_mat4_mul(view_proj, model, mvp);

	/* Use shader and set uniforms */
	glUseProgram(app->phong_shader);

	glUniformMatrix4fv(app->u_phong_mvp, 1, GL_FALSE, (float*)mvp);

	/* Set light direction - fixed from above */
	vec3 light_dir = {LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z};
	glm_vec3_normalize(light_dir);
	glUniform3f(app->u_phong_light_dir, light_dir[0], light_dir[1],
	            light_dir[2]);

	/* Draw icosphere */
	glBindVertexArray(app->vao);

	/* Only set polygon mode for the icosphere */
	glPolygonMode(GL_FRONT_AND_BACK, app->wireframe ? GL_LINE : GL_FILL);

	glDrawElements(GL_TRIANGLES, (GLsizei)app->geometry.indices.size,
	               GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action,
                         int mods)
{
	(void)scancode;
	(void)mods;

	App* app = (App*)glfwGetWindowUserPointer(window);

	if (action == GLFW_PRESS) {
		switch (key) {
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, GLFW_TRUE);
				break;
			case GLFW_KEY_W:
				app->wireframe = !app->wireframe;
				break;
			case GLFW_KEY_UP:
				if (app->subdivisions < MAX_SUBDIV) {
					app->subdivisions++;
				}
				break;
			case GLFW_KEY_DOWN:
				if (app->subdivisions > MIN_SUBDIV) {
					app->subdivisions--;
				}
				break;
			case GLFW_KEY_C:
				/* Toggle camera control */
				app->camera_enabled = !app->camera_enabled;
				if (app->camera_enabled) {
					glfwSetInputMode(window, GLFW_CURSOR,
					                 GLFW_CURSOR_DISABLED);
					app->first_mouse = 1;
				} else {
					glfwSetInputMode(window, GLFW_CURSOR,
					                 GLFW_CURSOR_NORMAL);
				}
				LOG_INFO("suckless-ogl.app", "Camera control: %s",
				         app->camera_enabled ? "ENABLED" : "DISABLED");
				break;
			case GLFW_KEY_SPACE:
				/* Reset camera to default position */
				app->camera_yaw = DEFAULT_CAMERA_ANGLE;
				app->camera_pitch = DEFAULT_CAMERA_ANGLE;
				app->camera_distance = DEFAULT_CAMERA_DISTANCE;
				app->env_lod = DEFAULT_ENV_LOD;
				LOG_INFO("suckless-ogl.app", "Camera and LOD reset");
				break;
			case GLFW_KEY_PAGE_UP:
				app->env_lod += LOD_STEP;
				if (app->env_lod > MAX_ENV_LOD) {
					app->env_lod = MAX_ENV_LOD;
				}
				LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
				         app->env_lod);
				break;
			case GLFW_KEY_PAGE_DOWN:
				app->env_lod -= LOD_STEP;
				if (app->env_lod < MIN_ENV_LOD) {
					app->env_lod = MIN_ENV_LOD;
				}
				LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
				         app->env_lod);
				break;
			case GLFW_KEY_F:
				app_toggle_fullscreen(app, window);
				break;
			default:
				/* Ignore other keys */
				break;
		}
	}
}

static void app_toggle_fullscreen(App* app, GLFWwindow* window)
{
	static const int REFRESH_RATE_WINDOWED = 0;

	if (app->is_fullscreen == 0) {
		/* Switch to fullscreen */
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		/* Save window geometry */
		glfwGetWindowPos(window, &app->saved_x, &app->saved_y);
		glfwGetWindowSize(window, &app->saved_width, &app->saved_height);

		glfwSetWindowMonitor(window, monitor, 0, 0, mode->width,
		                     mode->height, mode->refreshRate);
		app->is_fullscreen = 1;
		LOG_INFO("suckless-ogl.app", "Switched to fullscreen (%dx%d)",
		         mode->width, mode->height);
	} else {
		/* Switch back to windowed */
		glfwSetWindowMonitor(window, NULL, app->saved_x, app->saved_y,
		                     app->saved_width, app->saved_height,
		                     REFRESH_RATE_WINDOWED);
		app->is_fullscreen = 0;
		LOG_INFO("suckless-ogl.app", "Switched to windowed");
	}
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	App* app = (App*)glfwGetWindowUserPointer(window);

	if (!app->camera_enabled) {
		return;
	}

	/* Handle first mouse movement */
	if (app->first_mouse) {
		app->last_mouse_x = xpos;
		app->last_mouse_y = ypos;
		app->first_mouse = 0;
		return;
	}

	/* Calculate mouse delta */
	double delta_x = xpos - app->last_mouse_x;
	double delta_y = ypos - app->last_mouse_y;
	app->last_mouse_x = xpos;
	app->last_mouse_y = ypos;

	/* Update camera rotation (note: -delta_x for natural mouse movement) */
	app->camera_yaw += (float)(-delta_x * MOUSE_SENSITIVITY);
	app->camera_pitch += (float)(-delta_y * MOUSE_SENSITIVITY);

	/* Clamp pitch to avoid gimbal lock */
	if (app->camera_pitch > MAX_PITCH) {
		app->camera_pitch = MAX_PITCH;
	}
	if (app->camera_pitch < MIN_PITCH) {
		app->camera_pitch = MIN_PITCH;
	}
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	(void)xoffset;
	App* app = (App*)glfwGetWindowUserPointer(window);

	/* Zoom in/out with mouse wheel */
	app->camera_distance -= (float)yoffset * ZOOM_STEP;

	/* Clamp distance */
	if (app->camera_distance < MIN_CAMERA_DISTANCE) {
		app->camera_distance = MIN_CAMERA_DISTANCE;
	}
	if (app->camera_distance > MAX_CAMERA_DISTANCE) {
		app->camera_distance = MAX_CAMERA_DISTANCE;
	}
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	App* app = (App*)glfwGetWindowUserPointer(window);
	app->width = width;
	app->height = height;
	glViewport(0, 0, width, height);
}
