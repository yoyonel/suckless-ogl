#include "app.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MIN_SUBDIV 0
#define MAX_SUBDIV 6
#define CUBEMAP_SIZE 1024
#define MOUSE_SENSITIVITY 0.002f
#define MIN_PITCH -1.5f
#define MAX_PITCH 1.5f

static void key_callback(GLFWwindow* window, int key, int scancode, int action,
                         int mods);
static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

int app_init(App* app, int width, int height, const char* title)
{
	app->width = width;
	app->height = height;
	app->subdivisions = 2;
	app->wireframe = 0;

	/* Camera initial state */
	app->camera_yaw = 0.0f;
	app->camera_pitch = 0.0f;
	app->camera_distance = 3.0f;
	app->camera_enabled = 1; /* Enabled by default */
	app->first_mouse = 1;
	app->last_mouse_x = 0.0;
	app->last_mouse_y = 0.0;

	app->cubemap_size = CUBEMAP_SIZE;

	/* Initialize GLFW */
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
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
		fprintf(stderr, "Failed to create window\n");
		glfwTerminate();
		return 0;
	}

	glfwMakeContextCurrent(app->window);
	glfwSetWindowUserPointer(app->window, app);
	glfwSetKeyCallback(app->window, key_callback);
	glfwSetCursorPosCallback(app->window, mouse_callback);
	glfwSetScrollCallback(app->window, scroll_callback);

	/* Enable mouse capture by default */
	if (app->camera_enabled) {
		glfwSetInputMode(app->window, GLFW_CURSOR,
		                 GLFW_CURSOR_DISABLED);
	}

	/* Initialize GLAD */
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		fprintf(stderr, "Failed to initialize GLAD\n");
		glfwDestroyWindow(app->window);
		glfwTerminate();
		return 0;
	}

	printf("OpenGL %s\n", glGetString(GL_VERSION));

	/* Load HDR texture */
	int hdr_w, hdr_h;
	app->hdr_texture = texture_load_hdr("assets/env.hdr", &hdr_w, &hdr_h);
	if (!app->hdr_texture) {
		fprintf(stderr, "Failed to load HDR texture\n");
		return 0;
	}
	printf("HDR loaded: %dx%d\n", hdr_w, hdr_h);

	/* Load shaders */
	app->compute_shader = shader_load_compute("shaders/equirect2cube.glsl");
	if (!app->compute_shader) {
		fprintf(stderr, "Failed to load compute shader\n");
		return 0;
	}

	app->phong_shader =
	    shader_load_program("shaders/phong.vert", "shaders/phong.frag");
	if (!app->phong_shader) {
		fprintf(stderr, "Failed to load phong shader\n");
		return 0;
	}

	app->skybox_shader = shader_load_program("shaders/background.vert",
	                                         "shaders/background.frag");
	if (!app->skybox_shader) {
		fprintf(stderr, "Failed to load skybox shader\n");
		return 0;
	}

	/* Build environment cubemap */
	app->env_cubemap = texture_build_env_cubemap(
	    app->hdr_texture, app->cubemap_size, app->compute_shader);
	if (!app->env_cubemap) {
		fprintf(stderr, "Failed to build environment cubemap\n");
		return 0;
	}
	printf("Environment cubemap generated (%dx%d)\n", app->cubemap_size,
	       app->cubemap_size);

	/* Initialize skybox */
	skybox_init(&app->skybox);

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
	glDeleteTextures(1, &app->env_cubemap);

	glDeleteProgram(app->phong_shader);
	glDeleteProgram(app->skybox_shader);
	glDeleteProgram(app->compute_shader);

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
			             app->geometry.vertices.size * sizeof(vec3),
			             app->geometry.vertices.data,
			             GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			                      sizeof(vec3), (void*)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, app->nbo);
			glBufferData(GL_ARRAY_BUFFER,
			             app->geometry.normals.size * sizeof(vec3),
			             app->geometry.normals.data,
			             GL_STATIC_DRAW);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
			                      sizeof(vec3), (void*)0);
			glEnableVertexAttribArray(1);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->ebo);
			glBufferData(
			    GL_ELEMENT_ARRAY_BUFFER,
			    app->geometry.indices.size * sizeof(unsigned int),
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
	glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* Calculate camera position from yaw, pitch, and distance */
	float cam_x = app->camera_distance * cosf(app->camera_pitch) *
	              sinf(app->camera_yaw);
	float cam_y = app->camera_distance * sinf(app->camera_pitch);
	float cam_z = app->camera_distance * cosf(app->camera_pitch) *
	              cosf(app->camera_yaw);

	/* Setup camera matrices */
	mat4 view, proj, view_proj, inv_view_proj;
	vec3 camera_pos = {cam_x, cam_y, cam_z};
	vec3 target = {0.0f, 0.0f, 0.0f};
	vec3 up = {0.0f, 1.0f, 0.0f};

	glm_lookat(camera_pos, target, up, view);
	glm_perspective(glm_rad(60.0f), (float)app->width / (float)app->height,
	                0.1f, 100.0f, proj);

	/* Calculate MVP for icosphere */
	glm_mat4_mul(proj, view, view_proj);

	/* For skybox: view matrix without translation */
	mat4 view_no_translation;
	glm_mat4_copy(view, view_no_translation);
	view_no_translation[3][0] = 0.0f; /* Remove X translation */
	view_no_translation[3][1] = 0.0f; /* Remove Y translation */
	view_no_translation[3][2] = 0.0f; /* Remove Z translation */

	/* Calculate inverse view-projection for skybox */
	glm_mat4_mul(proj, view_no_translation, inv_view_proj);
	glm_mat4_inv(inv_view_proj, inv_view_proj);

	/* Render skybox (without translation, stays centered) */
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	skybox_render(&app->skybox, app->skybox_shader, app->env_cubemap,
	              inv_view_proj, 3.0f);

	/* Render icosphere (with full view matrix including translation) */
	app_render_icosphere(app, view_proj);
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

	GLuint mvp_loc = glGetUniformLocation(app->phong_shader, "uMVP");
	glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, (float*)mvp);

	/* Set light direction - fixed from above */
	vec3 light_dir = {0.5f, 1.0f, 0.3f};
	glm_vec3_normalize(light_dir);
	GLuint light_loc = glGetUniformLocation(app->phong_shader, "lightDir");
	glUniform3f(light_loc, light_dir[0], light_dir[1], light_dir[2]);

	/* Draw icosphere */
	glBindVertexArray(app->vao);
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
				if (app->subdivisions < MAX_SUBDIV)
					app->subdivisions++;
				break;
			case GLFW_KEY_DOWN:
				if (app->subdivisions > MIN_SUBDIV)
					app->subdivisions--;
				break;
			case GLFW_KEY_C:
				/* Toggle camera control */
				app->camera_enabled = !app->camera_enabled;
				if (app->camera_enabled) {
					glfwSetInputMode(window, GLFW_CURSOR,
					                 GLFW_CURSOR_DISABLED);
					app->first_mouse =
					    1; /* Reset mouse for smooth
					          transition */
				} else {
					glfwSetInputMode(window, GLFW_CURSOR,
					                 GLFW_CURSOR_NORMAL);
				}
				printf("Camera control: %s\n",
				       app->camera_enabled ? "ENABLED"
				                           : "DISABLED");
				break;
			case GLFW_KEY_SPACE:
				/* Reset camera to default position */
				app->camera_yaw = 0.0f;
				app->camera_pitch = 0.0f;
				app->camera_distance = 3.0f;
				printf("Camera reset\n");
				break;
		}
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
	double dx = xpos - app->last_mouse_x;
	double dy = ypos - app->last_mouse_y;
	app->last_mouse_x = xpos;
	app->last_mouse_y = ypos;

	/* Update camera rotation (note: -dx for natural mouse movement) */
	app->camera_yaw += (float)(-dx * MOUSE_SENSITIVITY);
	app->camera_pitch += (float)(-dy * MOUSE_SENSITIVITY);

	/* Clamp pitch to avoid gimbal lock */
	if (app->camera_pitch > MAX_PITCH)
		app->camera_pitch = MAX_PITCH;
	if (app->camera_pitch < MIN_PITCH)
		app->camera_pitch = MIN_PITCH;
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	(void)xoffset;
	App* app = (App*)glfwGetWindowUserPointer(window);

	/* Zoom in/out with mouse wheel */
	app->camera_distance -= (float)yoffset * 0.2f;

	/* Clamp distance */
	if (app->camera_distance < 1.5f)
		app->camera_distance = 1.5f;
	if (app->camera_distance > 10.0f)
		app->camera_distance = 10.0f;
}
