#include "app.h"

#include "fps.h"
#include "glad/glad.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif
#include "log.h"
#include "material.h"
#include "pbr.h"
#include "perf_timer.h"
#include "shader.h"
#include "skybox.h"
#include "texture.h"
#include "ui.h"
#include <GLFW/glfw3.h>
#include <cglm/affine.h>  // IWYU pragma: keep
#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <cglm/vec3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>  // for malloc

#define MOUSE_SENSITIVITY 0.002F
#define MIN_PITCH -1.5F
#define MAX_PITCH 1.5F
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// #define USE_RAYTRACE_BILLBOARD

enum {
	MIN_SUBDIV = 0,
	MAX_SUBDIV = 6,
	CUBEMAP_SIZE = 1024,
	INITIAL_SUBDIVISIONS = 3
};

static const float DEFAULT_CAMERA_DISTANCE = 20.0F;
static const float DEFAULT_ENV_LOD = 0.0F;
static const float DEFAULT_CAMERA_ANGLE = 0.0F;
static const float NEAR_PLANE = 0.1F;
static const float FAR_PLANE = 100.0F;
static const float FOV_ANGLE = 60.0F;
static const float MAX_ENV_LOD = 10.0F;
static const float MIN_ENV_LOD = 0.0F;
static const float LOD_STEP = 0.5F;
static const float MIN_CAMERA_DISTANCE = 1.5F;
static const float MAX_CAMERA_DISTANCE = 50.0F;
static const float ZOOM_STEP = 0.2F;
static const float LIGHT_DIR_X = 0.5F;
static const float LIGHT_DIR_Y = 1.0F;
static const float LIGHT_DIR_Z = 0.3F;
// PBR+IBL
static const int PREFILTERED_SPECULAR_MAP_SIZE = 1024;
static const int IRIDIANCE_MAP_SIZE = 64;
static const int BRDF_LUT_MAP_SIZE = 512;
static const float DEFAULT_CLAMP_MULTIPLIER = 6.0F;
static const float DEFAULT_METALLIC = 1.0F;
static const float DEFAULT_ROUGHNESS = 0.0F;
static const float DEFAULT_AO = 1.0F;
static const float DEFAULT_EXPOSURE = 1.0F;
//
static const float DEFAULT_FONT_SIZE = 32.0F;
static const float DEFAULT_FPS_SMOOTHING = 0.95F;
static const float DEFAULT_FPS_WINDOW = 5.0F;
//
static const int DEFAULT_COLS = 10;
static const float DEFAULT_SPACING = 2.5F;
static const float HALF_OFFSET_MULTIPLIER = 0.5F;
//
static const float DEFAULT_FONT_SHADOW_OFFSET_X = 2.0F;
static const float DEFAULT_FONT_SHADOW_OFFSET_Y = 2.0F;
static const float DEFAULT_FONT_OFFSET_X = 0.0F;
static const float DEFAULT_FONT_OFFSET_Y = 0.0F;
static const vec3 DEFAULT_FONT_COLOR = {1.0F, 1.0F, 1.0F};
static const vec3 DEFAULT_FONT_SHADOW_COLOR = {0.0F, 0.0F, 0.0F};
static const int MAX_FPS_TEXT_LENGTH = 64;
//
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
	app->camera_enabled = 1;        /* Enabled by default */
	app->env_lod = DEFAULT_ENV_LOD; /* Default blur level */
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

	PERF_MEASURE_LOG("Asset Loading Time (CPU + Upload)")
	{
		app->hdr_texture =
		    texture_load_hdr("assets/env.hdr", &hdr_w, &hdr_h);
	}
	if (!app->hdr_texture) {
		LOG_ERROR("suckless-ogl.app", "Failed to load HDR texture");
		return 0;
	}

	GPU_MEASURE_LOG("PBR Generation Time (GPU Compute)")
	{
		app->spec_prefiltered_tex = build_prefiltered_specular_map(
		    app->hdr_texture, PREFILTERED_SPECULAR_MAP_SIZE,
		    PREFILTERED_SPECULAR_MAP_SIZE / 2);

		float auto_threshold = compute_mean_luminance_gpu(
		    app->hdr_texture, hdr_w, hdr_h, DEFAULT_CLAMP_MULTIPLIER);
		LOG_INFO("suckless-ogl.ibl",
		         "Computed adaptive clamp threshold: %.2f",
		         auto_threshold);

		app->irradiance_tex = build_irradiance_map(
		    app->hdr_texture, IRIDIANCE_MAP_SIZE, auto_threshold);

		app->brdf_lut_tex = build_brdf_lut_map(BRDF_LUT_MAP_SIZE);
	}

	app->pbr_shader =
	    shader_load_program("shaders/pbr_ibl.vert", "shaders/pbr_ibl.frag");
	if (!app->pbr_shader) {
		LOG_ERROR("suckless-ogl.app", "Failed to load pbr shader");
		return 0;
	}
	app->pbr_rt_shader = shader_load_program("shaders/pbr_ibl_rt.vert",
	                                         "shaders/pbr_ibl_rt.frag");
	if (!app->pbr_rt_shader) {
		LOG_ERROR("suckless-ogl.app", "Failed to load pbr rt shader");
		return 0;
	}

	app->u_metallic = DEFAULT_METALLIC;
	app->u_roughness = DEFAULT_ROUGHNESS;
	app->u_ao = DEFAULT_AO;
	app->u_exposure = DEFAULT_EXPOSURE;

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

	//
	app->debug_shader = shader_load_program("shaders/debug_tex.vert",
	                                        "shaders/debug_tex.frag");
	glGenVertexArrays(1, &app->empty_vao);
	app->debug_lod = 0.0F;
	app->show_debug_tex = false;

	if (!app->debug_shader) {
		LOG_ERROR("suckless-ogl.app", "Failed to load debug shader");
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
	glGenVertexArrays(1, &app->sphere_vao);
	glGenBuffers(1, &app->sphere_vbo);
	glGenBuffers(1, &app->sphere_nbo);
	glGenBuffers(1, &app->sphere_ebo);

	app->quad_indices_size = 0;
	app_init_quad(app);

	/* Enable depth testing */
	glEnable(GL_DEPTH_TEST);

	fps_init(&app->fps_counter, DEFAULT_FPS_SMOOTHING, DEFAULT_FPS_WINDOW);
	app->last_frame_time = glfwGetTime();

	ui_init(&app->ui, "assets/fonts/FiraCode-Regular.ttf",
	        DEFAULT_FONT_SIZE);

	app->material_lib =
	    material_load_presets("assets/materials/pbr_materials.json");

#ifdef USE_SSBO_RENDERING
	app_init_ssbo(app);
	app->pbr_ssbo_shader = shader_load_program("shaders/pbr_ibl_ssbo.vert",
	                                           "shaders/pbr_ibl_ssbo.frag");
	if (!app->pbr_ssbo_shader) {
		LOG_ERROR("suckless-ogl.app", "Failed to load pbr_ssbo shader");
		return 0;
	}
	LOG_INFO("suckless-ogl.app", "SSBO rendering mode active");
#else
	app_init_instancing(app);
	app->pbr_instanced_shader = shader_load_program(
	    "shaders/pbr_ibl_instanced.vert", "shaders/pbr_ibl_instanced.frag");
	if (!app->pbr_instanced_shader) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to load pbr_instanced shader");
		return 0;
	}
	LOG_INFO("suckless-ogl.app", "Legacy instanced rendering mode active");
#endif

	return 1;
}

#ifdef USE_SSBO_RENDERING
void app_init_ssbo(App* app)
{
	const int total_count =
	    MIN(app->material_lib->count, DEFAULT_COLS * DEFAULT_COLS);
	const int cols = DEFAULT_COLS;
	const int rows = (total_count + cols - 1) / cols;
	const float spacing = DEFAULT_SPACING;

	const float grid_w = (float)(cols - 1) * spacing;
	const float grid_h = (float)(rows - 1) * spacing;

	SphereInstanceSSBO* data =
	    malloc(sizeof(SphereInstanceSSBO) * total_count);
	if (!data) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to allocate memory for SSBO");
		return;
	}

	for (int i = 0; i < total_count; i++) {
		const int grid_x = i % cols;
		const int grid_y = i / cols;

		glm_mat4_identity(data[i].model);

		const float pos_x = ((float)grid_x * spacing) -
		                    (grid_w * HALF_OFFSET_MULTIPLIER);
		const float pos_y = -(((float)grid_y * spacing) -
		                      (grid_h * HALF_OFFSET_MULTIPLIER));

		vec3 position = {pos_x, pos_y, 0.0F};
		glm_translate(data[i].model, position);

		PBRMaterial* mat = &app->material_lib->materials[i];

		glm_vec3_copy(mat->albedo, data[i].albedo);
		data[i].metallic = mat->metallic;
		data[i].roughness = mat->roughness;
		data[i].ao = 1.0F;
		data[i]._padding[0] = 0.0F;
		data[i]._padding[1] = 0.0F;
	}

	/* Debug : vérifier la première instance */
	LOG_DEBUG("suckless-ogl.ssbo",
	          "First instance - pos: (%.2f, %.2f, %.2f), albedo: (%.2f, "
	          "%.2f, %.2f)",
	          data[0].model[3][0], data[0].model[3][1], data[0].model[3][2],
	          data[0].albedo[0], data[0].albedo[1], data[0].albedo[2]);

	ssbo_group_init(&app->ssbo_group, data, total_count);
	ssbo_group_bind_mesh(&app->ssbo_group, app->sphere_vbo, app->sphere_nbo,
	                     app->sphere_ebo);

	free(data);
}
#endif

void app_init_instancing(App* app)
{
	// 1. Calcul des dimensions (identique au legacy)
	const int total_count =
	    MIN(app->material_lib->count, DEFAULT_COLS * DEFAULT_COLS);
	const int cols = DEFAULT_COLS;
	const int rows = (total_count + cols - 1) / cols;
	const float spacing = DEFAULT_SPACING;

	const float grid_w = (float)(cols - 1) * spacing;
	const float grid_h = (float)(rows - 1) * spacing;

	// 2. Allocation temporaire pour le transfert
	SphereInstance* data = malloc(sizeof(SphereInstance) * total_count);
	if (!data) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to allocate memory for instancing");
		return;
	}

	for (int i = 0; i < total_count; i++) {
		const int grid_x = i % cols;
		const int grid_y = i / cols;

		// Calcul de la matrice Model (Logique de centrage reproduite)
		glm_mat4_identity(data[i].model);

		const float pos_x = ((float)grid_x * spacing) -
		                    (grid_w * HALF_OFFSET_MULTIPLIER);
		const float pos_y = -(((float)grid_y * spacing) -
		                      (grid_h * HALF_OFFSET_MULTIPLIER));

		vec3 position = {pos_x, pos_y, 0.0F};
		// NOLINTNEXTLINE(misc-include-cleaner)
		glm_translate(data[i].model, position);

		// Récupération des propriétés du matériau depuis la
		// bibliothèque
		PBRMaterial* mat = &app->material_lib->materials[i];

		glm_vec3_copy(mat->albedo, data[i].albedo);
		data[i].metallic = mat->metallic;
		data[i].roughness = mat->roughness;
		data[i].ao = 1.0F;  // Valeur par défaut
	}

	// 3. Initialisation du groupe (Transfert VBO Instance + Création VAO)
	instanced_group_init(&app->instanced_group, data, total_count);

	// 4. Premier lien avec la géométrie actuelle
	// Note: on utilise les noms de buffers de ton app.h (sphere_vbo, etc.)
	instanced_group_bind_mesh(&app->instanced_group, app->sphere_vbo,
	                          app->sphere_nbo, app->sphere_ebo);

	free(data);
}

void app_render_instanced(App* app, mat4 view, mat4 proj, vec3 camera_pos)
{
	GLuint id_current_shader = 0;
#ifdef USE_SSBO_RENDERING
	id_current_shader = app->pbr_ssbo_shader;
#else
	id_current_shader = app->pbr_instanced_shader;
#endif

	glUseProgram(id_current_shader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, app->irradiance_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, app->spec_prefiltered_tex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, app->brdf_lut_tex);

	glUniform1i(glGetUniformLocation(id_current_shader, "irradianceMap"),
	            0);
	glUniform1i(glGetUniformLocation(id_current_shader, "prefilterMap"), 1);
	glUniform1i(glGetUniformLocation(id_current_shader, "brdfLUT"), 2);

	glUniform3fv(glGetUniformLocation(id_current_shader, "camPos"), 1,
	             camera_pos);
	glUniformMatrix4fv(
	    glGetUniformLocation(id_current_shader, "projection"), 1, GL_FALSE,
	    (float*)proj);
	glUniformMatrix4fv(glGetUniformLocation(id_current_shader, "view"), 1,
	                   GL_FALSE, (float*)view);
	glUniform1f(glGetUniformLocation(id_current_shader, "pbr_exposure"),
	            app->u_exposure);

#ifdef USE_SSBO_RENDERING
	ssbo_group_draw(&app->ssbo_group, app->geometry.indices.size);
#else
	instanced_group_draw(&app->instanced_group, app->geometry.indices.size);
#endif
}

void app_init_quad(App* app)
{
	const float QUAD_VERTICES[] = {-1.0F, -1.0F, 0.0F, 1.0F,  -1.0F, 0.0F,
	                               1.0F,  1.0F,  0.0F, -1.0F, 1.0F,  0.0F};
	const unsigned int QUAD_INDICES[] = {0, 1, 2, 2, 3, 0};

	glGenVertexArrays(1, &app->quad_vao);
	glGenBuffers(1, &app->quad_vbo);
	glGenBuffers(1, &app->quad_ebo);

	glBindVertexArray(app->quad_vao);
	glBindBuffer(GL_ARRAY_BUFFER, app->quad_vbo);

	glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES,
	             GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->quad_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_INDICES),
	             QUAD_INDICES, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
	                      (void*)0);
	glEnableVertexAttribArray(0);

	app->quad_indices_size =
	    (GLsizei)sizeof(QUAD_INDICES) / sizeof(unsigned int);
}

void app_cleanup(App* app)
{
	icosphere_free(&app->geometry);
	skybox_cleanup(&app->skybox);

	glDeleteVertexArrays(1, &app->sphere_vao);
	glDeleteBuffers(1, &app->sphere_vbo);
	glDeleteBuffers(1, &app->sphere_nbo);
	glDeleteBuffers(1, &app->sphere_ebo);

	ui_destroy(&app->ui);

	glDeleteTextures(1, &app->hdr_texture);
	glDeleteProgram(app->phong_shader);
	glDeleteProgram(app->skybox_shader);

	material_free_lib(app->material_lib);

#ifdef USE_SSBO_RENDERING
	ssbo_group_cleanup(&app->ssbo_group);
#else
	instanced_group_cleanup(&app->instanced_group);
#endif

	glfwDestroyWindow(app->window);
	glfwTerminate();
}

void app_run(App* app)
{
	int last_subdiv = -1;

	while (!glfwWindowShouldClose(app->window)) {
		double current_time = glfwGetTime();
		double delta_time = current_time - app->last_frame_time;
		app->last_frame_time = current_time;
		fps_update(&app->fps_counter, delta_time, current_time);

		/* Regenerate icosphere if subdivision level changed */
		if (app->subdivisions != last_subdiv) {
			icosphere_generate(&app->geometry, app->subdivisions);

			/* Upload to GPU */
			app_update_gpu_buffers(app);

#ifdef USE_SSBO_RENDERING
			ssbo_group_bind_mesh(&app->ssbo_group, app->sphere_vbo,
			                     app->sphere_nbo, app->sphere_ebo);
#else
			instanced_group_bind_mesh(
			    &app->instanced_group, app->sphere_vbo,
			    app->sphere_nbo, app->sphere_ebo);
#endif

			last_subdiv = app->subdivisions;
		}

		app_render(app);

		glfwSwapBuffers(app->window);
		glfwPollEvents();
	}
}

void app_update_gpu_buffers(App* app)
{
	glBindVertexArray(app->sphere_vao);

	glBindBuffer(GL_ARRAY_BUFFER, app->sphere_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(app->geometry.vertices.size * sizeof(vec3)),
	             app->geometry.vertices.data, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, app->sphere_nbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(app->geometry.normals.size * sizeof(vec3)),
	             app->geometry.normals.data, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->sphere_ebo);
	glBufferData(
	    GL_ELEMENT_ARRAY_BUFFER,
	    (GLsizeiptr)(app->geometry.indices.size * sizeof(unsigned int)),
	    app->geometry.indices.data, GL_STATIC_DRAW);

	glBindVertexArray(0);
}

void app_render(App* app)
{
	/* Explicitly clear color and depth.
	   Many Intel drivers perform better with color clear than without it.
	 */
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (app->show_debug_tex) {
		glUseProgram(app->debug_shader);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, app->brdf_lut_tex);
		glUniform1i(glGetUniformLocation(app->debug_shader, "tex"), 0);
		glUniform1f(glGetUniformLocation(app->debug_shader, "lod"),
		            app->debug_lod);
		glBindVertexArray(app->empty_vao);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);
		return;
	}

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
	glm_perspective(glm_rad(FOV_ANGLE),
	                (float)app->width / (float)app->height, NEAR_PLANE,
	                FAR_PLANE, proj);

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
	glPolygonMode(GL_FRONT_AND_BACK, app->wireframe ? GL_LINE : GL_FILL);
	//
	// app_render_icosphere(app, view_proj);
	// app_render_icosphere_pbr(app, view, proj, camera_pos);
	app_render_instanced(app, view, proj, camera_pos);

	/* 2. Render skybox LAST (using LEQUAL to fill background) */
	/* We always use FILL for the skybox regardless of wireframe mode */
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	skybox_render(&app->skybox, app->skybox_shader, app->hdr_texture,
	              inv_view_proj, app->env_lod);
	// skybox_render(&app->skybox, app->skybox_shader,
	//               app->spec_prefiltered_tex, inv_view_proj,
	//               app->env_lod);
	// skybox_render(&app->skybox, app->skybox_shader, app->irradiance_tex,
	//               inv_view_proj, app->env_lod);

	app_render_ui(app);
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
	glBindVertexArray(app->sphere_vao);
	glDrawElements(GL_TRIANGLES, (GLsizei)app->geometry.indices.size,
	               GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void app_render_icosphere_pbr(App* app, mat4 view, mat4 proj, vec3 camera_pos)
{
	glUseProgram(app->pbr_shader);

	// Bind des textures une seule fois avant la boucle (Optimisation)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, app->irradiance_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, app->spec_prefiltered_tex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, app->brdf_lut_tex);

	glUniform1i(glGetUniformLocation(app->pbr_shader, "irradianceMap"), 0);
	glUniform1i(glGetUniformLocation(app->pbr_shader, "prefilterMap"), 1);
	glUniform1i(glGetUniformLocation(app->pbr_shader, "brdfLUT"), 2);
	glUniform3fv(glGetUniformLocation(app->pbr_shader, "camPos"), 1,
	             camera_pos);
	glUniformMatrix4fv(glGetUniformLocation(app->pbr_shader, "projection"),
	                   1, GL_FALSE, (float*)proj);
	glUniformMatrix4fv(glGetUniformLocation(app->pbr_shader, "view"), 1,
	                   GL_FALSE, (float*)view);

	// Calcul de la grille
	const int total_count =
	    MIN(app->material_lib->count, DEFAULT_COLS * DEFAULT_COLS);
	const int cols = DEFAULT_COLS;
	const int rows = (total_count + cols - 1) / cols;
	const float spacing = DEFAULT_SPACING;

	// Calcul des dimensions totales pour le centrage
	const float grid_w = (float)(cols - 1) * spacing;
	const float grid_h = (float)(rows - 1) * spacing;

	mat4 invView;
	glm_mat4_inv(view, invView);

	for (int i = 0; i < total_count; i++) {
		const int grid_x = i % cols;
		const int grid_y = i / cols;

		mat4 model_matrix;
		glm_mat4_identity(model_matrix);

		// Centrage : (pos_brute - (taille_totale * 0.5))
		const float pos_x = ((float)grid_x * spacing) -
		                    (grid_w * HALF_OFFSET_MULTIPLIER);
		const float pos_y = -(((float)grid_y * spacing) -
		                      (grid_h * HALF_OFFSET_MULTIPLIER));

		vec3 position = {pos_x, pos_y, 0.0F};
		// NOLINTNEXTLINE(misc-include-cleaner)
		glm_translate(model_matrix, position);

		PBRMaterial* material = &app->material_lib->materials[i];

// On passe les paramètres au shader via la fonction d'instance
#ifndef USE_RAYTRACE_BILLBOARD
		app_render_pbr_instance(app, view, proj, camera_pos,
		                        model_matrix, material->metallic,
		                        material->roughness, material->albedo);
#else
		app_render_pbr_billboard(app, view, invView, proj, camera_pos,
		                         model_matrix, material->metallic,
		                         material->roughness, material->albedo);
#endif
	}
}

void app_render_pbr_billboard(App* app, mat4 view, mat4 invView, mat4 proj,
                              vec3 camera_pos, mat4 model, float metallic,
                              float roughness, vec3 albedo)
{
	glUseProgram(app->pbr_rt_shader);

	// Uniforms existants
	glUniform3fv(glGetUniformLocation(app->pbr_rt_shader, "camPos"), 1,
	             camera_pos);
	glUniformMatrix4fv(
	    glGetUniformLocation(app->pbr_rt_shader, "projection"), 1, GL_FALSE,
	    (float*)proj);
	glUniformMatrix4fv(glGetUniformLocation(app->pbr_rt_shader, "view"), 1,
	                   GL_FALSE, (float*)view);
	glUniformMatrix4fv(glGetUniformLocation(app->pbr_rt_shader, "model"), 1,
	                   GL_FALSE, (float*)model);
	glUniformMatrix4fv(glGetUniformLocation(app->pbr_rt_shader, "invView"),
	                   1, GL_FALSE, (float*)invView);

	// Nouveaux Uniforms Billboard
	glUniform1f(glGetUniformLocation(app->pbr_rt_shader, "SphereRadius"),
	            1.0F);
	// glUniform1f(
	//     glGetUniformLocation(app->pbr_rt_shader,
	//     "aa_width_multiplier"), 1.0f);
	// glUniform1f(glGetUniformLocation(app->pbr_rt_shader,
	// "aa_distance_factor"),
	//             0.002f);

	// Paramètres PBR
	glUniform3f(glGetUniformLocation(app->pbr_rt_shader, "material.albedo"),
	            albedo[0], albedo[1], albedo[2]);
	glUniform1f(
	    glGetUniformLocation(app->pbr_rt_shader, "material.metallic"),
	    metallic);
	glUniform1f(
	    glGetUniformLocation(app->pbr_rt_shader, "material.roughness"),
	    roughness);
	glUniform1f(glGetUniformLocation(app->pbr_rt_shader, "material.ao"),
	            app->u_ao);
	glUniform1f(glGetUniformLocation(app->pbr_rt_shader, "pbr_exposure"),
	            app->u_exposure);

	// Activer le blending pour l'AA analytique
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindVertexArray(app->quad_vao);
	glDrawElements(GL_TRIANGLES, app->quad_indices_size, GL_UNSIGNED_INT,
	               0);

	glDisable(GL_BLEND);
}

void app_render_pbr_instance(App* app, mat4 view, mat4 proj, vec3 camera_pos,
                             mat4 model, float metallic, float roughness,
                             vec3 albedo)
{
	glUseProgram(app->pbr_shader);

	// 1. Matrices
	glUniformMatrix4fv(glGetUniformLocation(app->pbr_shader, "projection"),
	                   1, GL_FALSE, (float*)proj);
	glUniformMatrix4fv(glGetUniformLocation(app->pbr_shader, "view"), 1,
	                   GL_FALSE, (float*)view);
	glUniformMatrix4fv(glGetUniformLocation(app->pbr_shader, "model"), 1,
	                   GL_FALSE, (float*)model);
	glUniform3fv(glGetUniformLocation(app->pbr_shader, "camPos"), 1,
	             camera_pos);

	// 2. Paramètres
	glUniform3f(glGetUniformLocation(app->pbr_shader, "material.albedo"),
	            albedo[0], albedo[1], albedo[2]);
	glUniform1f(glGetUniformLocation(app->pbr_shader, "material.metallic"),
	            metallic);
	glUniform1f(glGetUniformLocation(app->pbr_shader, "material.roughness"),
	            roughness);
	glUniform1f(glGetUniformLocation(app->pbr_shader, "material.ao"),
	            app->u_ao);
	glUniform1f(glGetUniformLocation(app->pbr_shader, "pbr_exposure"),
	            app->u_exposure);

	// LOG_DEBUG("suckless-ogl.app", "Metallic: %.1F, Roughness: %.1F",
	//           metallic, roughness);

	// 3. Textures (On peut optimiser en ne les bindant qu'une fois par
	// frame hors de la boucle)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, app->irradiance_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, app->spec_prefiltered_tex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, app->brdf_lut_tex);

	// 4. Draw
	glBindVertexArray(app->sphere_vao);
	glDrawElements(GL_TRIANGLES, (GLsizei)app->geometry.indices.size,
	               GL_UNSIGNED_INT, 0);
}

void app_render_ui(App* app)
{
	char fps_text[MAX_FPS_TEXT_LENGTH];
	double fps = 0.0F;
	if (app->fps_counter.average_frame_time > 0) {
		fps = 1.0F / app->fps_counter.average_frame_time;
	}
	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	(void)snprintf(fps_text, sizeof(fps_text), "FPS: %.1F", fps);

	// Paramètres OpenGL pour le texte (Alpha blending)
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	// 1. DESSINER L'OMBRE (Décalée de 2 pixels, en Noir)
	ui_draw_text(&app->ui, fps_text, DEFAULT_FONT_SHADOW_OFFSET_X,
	             DEFAULT_FONT_SHADOW_OFFSET_Y,
	             (float*)DEFAULT_FONT_SHADOW_COLOR, app->width,
	             app->height);

	// 2. DESSINER LE TEXTE (En Jaune ou Blanc)
	ui_draw_text(&app->ui, fps_text, DEFAULT_FONT_OFFSET_X,
	             DEFAULT_FONT_OFFSET_Y, (float*)DEFAULT_FONT_COLOR,
	             app->width, app->height);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
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
				LOG_INFO("suckless-ogl.app",
				         "Camera control: %s",
				         app->camera_enabled ? "ENABLED"
				                             : "DISABLED");
				break;
			case GLFW_KEY_SPACE:
				/* Reset camera to default position */
				app->camera_yaw = DEFAULT_CAMERA_ANGLE;
				app->camera_pitch = DEFAULT_CAMERA_ANGLE;
				app->camera_distance = DEFAULT_CAMERA_DISTANCE;
				app->env_lod = DEFAULT_ENV_LOD;
				LOG_INFO("suckless-ogl.app",
				         "Camera and LOD reset");
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
		glfwGetWindowSize(window, &app->saved_width,
		                  &app->saved_height);

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
