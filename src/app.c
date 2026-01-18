#include "app.h"

#include "app_settings.h"
#include "fps.h"
#include "glad/glad.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif
#include "camera.h"
#include "log.h"
#include "material.h"
#include "pbr.h"
#include "perf_timer.h"
#include "postprocess.h"
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
#include <stdio.h>
#include <stdlib.h>  // for malloc

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void key_callback(GLFWwindow* window, int key, int scancode, int action,
                         int mods);
static void camera_process_key_callback(Camera* camera, int key, int action);
static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static void framebuffer_size_callback(GLFWwindow* window, int width,
                                      int height);
static void app_toggle_fullscreen(App* app, GLFWwindow* window);
static void app_save_raw_frame(App* app, const char* filename);

int app_init(App* app, int width, int height, const char* title)
{
	app->width = width;
	app->height = height;
	app->subdivisions = INITIAL_SUBDIVISIONS;
	app->wireframe = 0;

	/* Camera initial state */
	app->camera_enabled = 1;        /* Enabled by default */
	app->env_lod = DEFAULT_ENV_LOD; /* Default blur level */
	app->is_fullscreen = 0;
	app->first_mouse = 1;
	app->last_mouse_x = 0.0;
	app->last_mouse_y = 0.0;

	//
	camera_init(&app->camera, DEFAULT_CAMERA_DISTANCE, DEFAULT_CAMERA_YAW,
	            DEFAULT_CAMERA_PITCH);
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
	glfwWindowHint(GLFW_SAMPLES, DEFAULT_SAMPLES);

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
	LOG_INFO("suckless-ogl.init", "samples: %d", DEFAULT_SAMPLES);

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

	app->u_metallic = DEFAULT_METALLIC;
	app->u_roughness = DEFAULT_ROUGHNESS;
	app->u_ao = DEFAULT_AO;
	app->u_exposure = DEFAULT_EXPOSURE;

	/* Load shaders */
	app->skybox_shader = shader_load_program("shaders/background.vert",
	                                         "shaders/background.frag");

	if (!app->skybox_shader) {
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

	/* Initialize icosphere geometry */
	icosphere_init(&app->geometry);

	/* Create OpenGL buffers */
	glGenVertexArrays(1, &app->sphere_vao);
	glGenBuffers(1, &app->sphere_vbo);
	glGenBuffers(1, &app->sphere_nbo);
	glGenBuffers(1, &app->sphere_ebo);

	/* Enable depth testing */
	glEnable(GL_DEPTH_TEST);

	/* Enable multisampling */
	if (DEFAULT_SAMPLES > 1) {
		glEnable(GL_MULTISAMPLE);
	}

	fps_init(&app->fps_counter, DEFAULT_FPS_SMOOTHING, DEFAULT_FPS_WINDOW);
	app->last_frame_time = glfwGetTime();

	ui_init(&app->ui, "assets/fonts/FiraCode-Regular.ttf",
	        DEFAULT_FONT_SIZE);

	app->material_lib =
	    material_load_presets("assets/materials/pbr_materials.json");

#ifdef USE_SSBO_RENDERING
	app_init_ssbo(app);
	app->pbr_ssbo_shader = shader_load_program(
	    "shaders/pbr_ibl_ssbo.vert", "shaders/pbr_ibl_instanced.frag");
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

	/* Initialize post-processing */
	if (!postprocess_init(&app->postprocess, width, height)) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to initialize post-processing");
		return 0;
	}

	postprocess_disable(&app->postprocess, POSTFX_VIGNETTE);
	postprocess_disable(&app->postprocess, POSTFX_GRAIN);
	postprocess_disable(&app->postprocess, POSTFX_CHROM_ABBR);
	postprocess_set_exposure(&app->postprocess, DEFAULT_EXPOSURE);
	LOG_INFO("suckless-ogl.app", "Style: Aucun (rendu pur)");

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
	glDeleteProgram(app->skybox_shader);

	material_free_lib(app->material_lib);

#ifdef USE_SSBO_RENDERING
	ssbo_group_cleanup(&app->ssbo_group);
#else
	instanced_group_cleanup(&app->instanced_group);
#endif

	postprocess_cleanup(&app->postprocess);

	glfwDestroyWindow(app->window);
	glfwTerminate();
}

void app_run(App* app)
{
	int last_subdiv = -1;

	while (!glfwWindowShouldClose(app->window)) {
		double current_time = glfwGetTime();
		app->delta_time = current_time - app->last_frame_time;
		app->last_frame_time = current_time;
		fps_update(&app->fps_counter, app->delta_time, current_time);

		/* Mettre à jour le temps pour le post-processing (grain animé)
		 */
		postprocess_update_time(&app->postprocess,
		                        (float)app->delta_time);

		// 1. Mise à jour de la physique (clavier) avec fixed timestep
		app->camera.physics_accumulator += (float)app->delta_time;
		while (app->camera.physics_accumulator >=
		       app->camera.fixed_timestep) {
			camera_fixed_update(&app->camera);
			app->camera.physics_accumulator -=
			    app->camera.fixed_timestep;
		}

		// 2. Interpolation de la rotation (smoothing)
		float alpha = app->camera.rotation_smoothing;
		app->camera.yaw =
		    app->camera.yaw +
		    ((app->camera.yaw_target - app->camera.yaw) * alpha);
		app->camera.pitch =
		    app->camera.pitch +
		    ((app->camera.pitch_target - app->camera.pitch) * alpha);

		// 3. Mise à jour des vecteurs de la caméra
		camera_update_vectors(&app->camera);

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
	/* Commencer le rendu dans le framebuffer de post-processing */
	postprocess_begin(&app->postprocess);

	/* Explicitly clear color and depth.
	   Many Intel drivers perform better with color clear than without it.
	 */
	glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
	/* glClear handled by postprocess_begin */

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

		/* Terminer et appliquer le post-processing */
		postprocess_end(&app->postprocess);

		return;
	}

	/* Setup camera matrices */
	mat4 view;
	mat4 proj;
	mat4 view_proj;
	mat4 inv_view_proj;

	vec3 camera_pos = {app->camera.position[0], app->camera.position[1],
	                   app->camera.position[2]};
	camera_get_view_matrix(&app->camera, view);

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
	app_render_instanced(app, view, proj, camera_pos);

	/* 2. Render skybox LAST (using LEQUAL to fill background) */
	/* We always use FILL for the skybox regardless of wireframe mode */
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	skybox_render(&app->skybox, app->skybox_shader, app->hdr_texture,
	              inv_view_proj, app->env_lod);

	/* 3. Post-processing */
	postprocess_end(&app->postprocess);

	app_render_ui(app);
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

static void handle_postprocess_input(App* app, int key)
{
	switch (key) {
		/* Post-Processing Toggles */
		case GLFW_KEY_V: /* Toggle Vignette */
			postprocess_toggle(&app->postprocess, POSTFX_VIGNETTE);
			LOG_INFO("suckless-ogl.app", "Vignette: %s",
			         postprocess_is_enabled(&app->postprocess,
			                                POSTFX_VIGNETTE)
			             ? "ON"
			             : "OFF");
			break;

		case GLFW_KEY_G: /* Toggle Grain */
			postprocess_toggle(&app->postprocess, POSTFX_GRAIN);
			LOG_INFO("suckless-ogl.app", "Grain: %s",
			         postprocess_is_enabled(&app->postprocess, POSTFX_GRAIN)
			             ? "ON"
			             : "OFF");
			break;

		case GLFW_KEY_X: /* Toggle Chromatic Aberration */
			postprocess_toggle(&app->postprocess, POSTFX_CHROM_ABBR);
			LOG_INFO("suckless-ogl.app", "Chromatic Aberration: %s",
			         postprocess_is_enabled(&app->postprocess,
			                                POSTFX_CHROM_ABBR)
			             ? "ON"
			             : "OFF");
			break;

		case GLFW_KEY_KP_ADD: /* Augmenter l'exposition */
		{
			float current = app->postprocess.exposure.exposure;
			postprocess_set_exposure(&app->postprocess,
			                         current + DEFAULT_EXPOSURE_STEP);
			LOG_INFO("suckless-ogl.app", "Exposure: %.2f",
			         app->postprocess.exposure.exposure);
		} break;

		case GLFW_KEY_KP_SUBTRACT: /* Diminuer l'exposition */
		{
			float current = app->postprocess.exposure.exposure;
			postprocess_set_exposure(
			    &app->postprocess,
			    current > DEFAULT_MIN_EXPOSURE
			        ? current - DEFAULT_EXPOSURE_STEP
			        : DEFAULT_MIN_EXPOSURE);
			LOG_INFO("suckless-ogl.app", "Exposure: %.2f",
			         app->postprocess.exposure.exposure);
		} break;

		/* Presets pour le post-processing */
		case GLFW_KEY_1: /* Preset: Aucun */
			postprocess_apply_preset(&app->postprocess, &PRESET_DEFAULT);
			LOG_INFO("suckless-ogl.app", "Style: Aucun (rendu pur)");
			break;

		case GLFW_KEY_2: /* Preset: Subtle */
			postprocess_apply_preset(&app->postprocess, &PRESET_SUBTLE);
			LOG_INFO("suckless-ogl.app", "Style: Subtle");
			break;

		case GLFW_KEY_3: /* Preset: Cinématique */
			postprocess_apply_preset(&app->postprocess, &PRESET_CINEMATIC);
			LOG_INFO("suckless-ogl.app", "Style: Cinématique");
			break;

		case GLFW_KEY_4: /* Preset: Vintage */
			postprocess_apply_preset(&app->postprocess, &PRESET_VINTAGE);
			LOG_INFO("suckless-ogl.app", "Style: Vintage");
			break;

		case GLFW_KEY_5: /* Style: "Matrix" */
			postprocess_apply_preset(&app->postprocess, &PRESET_MATRIX);
			LOG_INFO("suckless-ogl.app", "Style: Matrix Grading");
			break;

		case GLFW_KEY_6: /* Style: "Noir et Blanc Contrasté" */
			postprocess_apply_preset(&app->postprocess, &PRESET_BW_CONTRAST);
			LOG_INFO("suckless-ogl.app", "Style: Noir & Blanc");
			break;

		case GLFW_KEY_0:
		case GLFW_KEY_KP_0:
			/* Reset complet */
			postprocess_apply_preset(&app->postprocess, &PRESET_DEFAULT);
			LOG_INFO("suckless-ogl.app",
			         "Color Grading: Reset to Defaults");
			break;

		default:
			break;
	}
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
			case GLFW_KEY_P:  // Handle 'P' for Screenshot/Capture
				app_save_raw_frame(app, "capture_frame.raw");
				break;
			case GLFW_KEY_Z:  // key 'W' on French layout keyboard
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
				camera_init(
				    &app->camera, DEFAULT_CAMERA_DISTANCE,
				    DEFAULT_CAMERA_YAW, DEFAULT_CAMERA_PITCH);
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
			/* Post-Processing */
			default:
				handle_postprocess_input(app, key);
				break;
		}
	}

	camera_process_key_callback(&app->camera, key, action);
}

static void camera_process_key_callback(Camera* camera, int key, int action)
{
	int pressed = (action != GLFW_RELEASE);
	if (key == GLFW_KEY_W) {
		camera->move_forward = pressed;
	}
	if (key == GLFW_KEY_S) {
		camera->move_backward = pressed;
	}
	if (key == GLFW_KEY_A) {
		camera->move_left = pressed;
	}
	if (key == GLFW_KEY_D) {
		camera->move_right = pressed;
	}
	if (key == GLFW_KEY_Q) {
		camera->move_up = pressed;
	}
	if (key == GLFW_KEY_E) {
		camera->move_down = pressed;
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
	camera_process_mouse(&app->camera, (float)delta_x, (float)delta_y);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	(void)xoffset;
	App* app = (App*)glfwGetWindowUserPointer(window);

	// On passe le yoffset directement à la caméra
	camera_process_scroll(&app->camera, (float)yoffset);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	App* app = (App*)glfwGetWindowUserPointer(window);
	app->width = width;
	app->height = height;
	glViewport(0, 0, width, height);

	/* Redimensionner le post-processing */
	postprocess_resize(&app->postprocess, width, height);
}

static void app_save_raw_frame(App* app, const char* filename)
{
	int width = app->width;
	int height = app->height;
	size_t size = width * height * 3;
	unsigned char* pixels = malloc(size);

	if (!pixels) {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to allocate memory for RAW capture");
		return;
	}

	// Use 1-byte alignment to handle any window resolution correctly
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	FILE* file = fopen(filename, "wb");
	if (file) {
		size_t result = fwrite(pixels, 1, size, file);
		if (result != size) {
			LOG_ERROR("suckless-ogl.app",
			          "Failed to write RAW frame to file: %s",
			          filename);
			return;
		}
		result = fclose(file);
		if (result != 0) {
			LOG_ERROR("suckless-ogl.app",
			          "Failed to close file for RAW capture: %s",
			          filename);
			return;
		}
		LOG_INFO("suckless-ogl.app", "RAW frame captured: %s",
		         filename);
	} else {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to open file for RAW capture");
	}

	free(pixels);
	// Reset alignment to default for other operations
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
}