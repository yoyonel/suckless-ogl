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
#include "postprocess_presets.h"
#include "shader.h"
#include "skybox.h"
#include "texture.h"
#include "ui.h"
#include "utils.h"
#include <GLFW/glfw3.h>
#include <cglm/affine.h>  // IWYU pragma: keep
#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <cglm/vec3.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>  // for malloc
#include <string.h>

enum { PBR_DEBUG_MODE_COUNT = 9 };
enum { MAX_PATH_LENGTH = 256 };
enum {
	DEBUG_TEXT_BUFFER_SIZE = 128,
	RANGE_TEXT_BUFFER_SIZE = 64,
	ENV_TEXT_BUFFER_SIZE = 256,
	EXPOSURE_TEXT_BUFFER_SIZE = 64
};
static const float GRAPH_TEXT_PADDING = 20.0F;
static const vec3 GRAPH_TEXT_COLOR = {0.8F, 0.8F, 0.8F};
static const float LUMINANCE_EPSILON = 0.0001F;
static const float DEBUG_TEXT_Y_OFFSET = DEFAULT_FONT_SIZE * 4.0F;
static const vec3 DEBUG_ORANGE_COLOR = {1.0F, 0.5F, 0.0F};
static const vec3 HISTO_BAR_COLOR_GREEN = {0.0F, 0.7F, 0.0F};
static const vec3 HISTO_BAR_COLOR_BLUE = {0.0F, 0.5F, 0.8F};
static const vec3 HISTO_BAR_COLOR_RED = {0.8F, 0.5F, 0.0F};
static const vec3 ENV_TEXT_COLOR = {0.7F, 0.7F, 0.7F};

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
static void app_scan_hdr_files(App* app);
static int app_load_env_map(App* app, const char* filename);
static void app_draw_help_overlay(App* app);
static void app_draw_debug_overlay(App* app);
static void draw_exposure_debug_text(App* app);
static int compute_luminance_histogram(App* app, int* buckets, int size,
                                       float* min_lum, float* max_lum);
static void draw_luminance_histogram_graph(App* app, const int* buckets,
                                           int size, float min_lum,
                                           float max_lum);

static int compare_strings(const void* string_a, const void* string_b)
{
	return strcmp(*(const char**)string_a, *(const char**)string_b);
}

static void app_scan_hdr_files(App* app)
{
	app->hdr_count = 0;
	app->hdr_files = NULL;
	app->current_hdr_index = -1;

	app->current_hdr_index = -1;

	DIR* dir_handle = NULL;
	struct dirent* entry = NULL;
	dir_handle = opendir("assets/textures/hdr");
	if (dir_handle) {
		while ((entry = readdir(dir_handle)) != NULL) {
			char* dot = strrchr(entry->d_name, '.');
			if (dot && strcmp(dot, ".hdr") == 0) {
				app->hdr_count++;
				app->hdr_files =
				    realloc(app->hdr_files,
				            app->hdr_count * sizeof(char*));
				app->hdr_files[app->hdr_count - 1] =
				    strdup(entry->d_name);
			}
		}
		closedir(dir_handle);

		/* Sort files alphabetically for deterministic order */
		if (app->hdr_count > 1) {
			qsort(app->hdr_files, app->hdr_count, sizeof(char*),
			      compare_strings);
		}
	} else {
		LOG_ERROR("suckless-ogl.app",
		          "Failed to open assets/textures/hdr directory!");
	}
	LOG_INFO("suckless-ogl.app", "Found %d HDR files.", app->hdr_count);
}

static int app_load_env_map(App* app, const char* filename)
{
	char path[MAX_PATH_LENGTH];
	(void)safe_snprintf(path, sizeof(path), "assets/textures/hdr/%s",
	                    filename);

	int hdr_w = 0;
	int hdr_h = 0;

	/* Cleanup old textures if simple reload */
	if (app->hdr_texture) {
		glDeleteTextures(1, &app->hdr_texture);
	}
	if (app->spec_prefiltered_tex) {
		glDeleteTextures(1, &app->spec_prefiltered_tex);
	}
	if (app->irradiance_tex) {
		glDeleteTextures(1, &app->irradiance_tex);
	}

	PERF_MEASURE_LOG("Asset Loading Time (CPU + Upload)")
	{
		app->hdr_texture = texture_load_hdr(path, &hdr_w, &hdr_h);
	}
	if (!app->hdr_texture) {
		LOG_ERROR("suckless-ogl.app", "Failed to load HDR texture: %s",
		          path);
		return 0;
	}

	float auto_threshold = compute_mean_luminance_gpu(
	    app->hdr_texture, hdr_w, hdr_h, DEFAULT_CLAMP_MULTIPLIER);
	LOG_INFO("suckless-ogl.ibl",
	         "Auto threshold from compute_mean_luminance_gpu: %.2f",
	         auto_threshold);
	if (auto_threshold < 1.0F || isnan(auto_threshold) ||
	    isinf(auto_threshold)) {
		auto_threshold = DEFAULT_AUTO_THRESHOLD;
		LOG_WARN("suckless-ogl.ibl",
		         "Invalid auto_threshold detected. Using default: %.2f",
		         auto_threshold);
	}

	/* Store for later use in preset resets */
	app->auto_threshold = auto_threshold;

	PERF_MEASURE_LOG("Prefiltered Map Generation")
	{
		app->spec_prefiltered_tex = build_prefiltered_specular_map(
		    app->hdr_texture, PREFILTERED_SPECULAR_MAP_SIZE,
		    PREFILTERED_SPECULAR_MAP_SIZE, auto_threshold);
	}

	PERF_MEASURE_LOG("Irradiance Map Generation")
	{
		app->irradiance_tex = build_irradiance_map(
		    app->hdr_texture, IRIDIANCE_MAP_SIZE, auto_threshold);
	}

	LOG_INFO("suckless-ogl.app", "Loaded Environment: %s (Thresh: %.2f)",
	         filename, auto_threshold);

	/* Update postprocess exposure to match new environment */
	postprocess_set_exposure(&app->postprocess, auto_threshold);

	return 1;
}

int app_init(App* app, int width, int height, const char* title)
{
	app->width = width;
	app->height = height;
	app->subdivisions = INITIAL_SUBDIVISIONS;
	app->wireframe = 0;

	/* Camera initial state */
	app->camera_enabled = 1;        /* Enabled by default */
	app->env_lod = DEFAULT_ENV_LOD; /* Default blur level */
	app->show_info_overlay = 1;
	app->show_exposure_debug = 0;
	app->text_overlay_mode = 0; /* Off by default */
	app->pbr_debug_mode = 0;
	app->is_fullscreen = 0;
	app->show_help = 0; /* Hidden by default */
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
	/* Async PBO Init */
	glGenBuffers(1, &app->exposure_pbo);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, app->exposure_pbo);
	glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(float), NULL, GL_STREAM_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	app->current_exposure = 1.0F;

	LOG_INFO("suckless_ogl.context.base.window", "code: 450");

	/* Scan & Load HDR Environment */
	app->brdf_lut_tex =
	    build_brdf_lut_map(BRDF_LUT_MAP_SIZE); /* BRDF is constant */

	app_scan_hdr_files(app);
	if (app->hdr_count > 0) {
		/* Try to find 'env.hdr' as default, otherwise pick first */
		int default_idx = 0;
		for (int i = 0; i < app->hdr_count; i++) {
			if (strcmp(app->hdr_files[i], "env.hdr") == 0) {
				default_idx = i;
				break;
			}
		}
		app->current_hdr_index = default_idx;
		app_load_env_map(app, app->hdr_files[default_idx]);
	} else {
		LOG_ERROR("suckless-ogl.init",
		          "No HDR files found in assets/textures/hdr/!");
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
	postprocess_disable(&app->postprocess, POSTFX_AUTO_EXPOSURE);
	postprocess_enable(&app->postprocess, POSTFX_EXPOSURE);
	postprocess_enable(&app->postprocess, POSTFX_COLOR_GRADING);
	postprocess_set_exposure(&app->postprocess, app->auto_threshold);
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

	/* Pass PBR Debug Mode */
	glUniform1i(glGetUniformLocation(id_current_shader, "debugMode"),
	            app->pbr_debug_mode);

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
	glDeleteBuffers(1, &app->exposure_pbo);

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

static void app_draw_help_overlay(App* app)
{
	/* Setup strict 2D state again just in case */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	static const float HELP_START_X = 20.0F;
	static const float HELP_START_Y = 60.0F;
	static const float HELP_PADDING = 5.0F;
	static const float HELP_SECTION_PADDING = 10.0F;
	static const vec3 HELP_COLOR = {0.1F, 1.0F, 0.25F}; /* Yellow-ish */

	UILayout layout;
	ui_layout_init(&layout, &app->ui, HELP_START_X, HELP_START_Y,
	               HELP_PADDING, app->width, app->height);

	/* Section: Controls */
	ui_layout_text(&layout, "--- Controls ---", HELP_COLOR);
	ui_layout_text(&layout, "[WASD] Move", HELP_COLOR);
	ui_layout_text(&layout, "[Mouse] Look", HELP_COLOR);
	ui_layout_text(&layout, "[Scroll] Speed/Zoom", HELP_COLOR);
	ui_layout_text(&layout, "[Left Click] Capture Mouse", HELP_COLOR);
	ui_layout_text(&layout, "[ESC] Release Mouse/Exit", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: Features */
	ui_layout_text(&layout, "--- Features ---", HELP_COLOR);
	ui_layout_text(&layout, "[F1] Cycle Text Overlays", HELP_COLOR);
	ui_layout_text(&layout, "[F2] Toggle Help", HELP_COLOR);
	ui_layout_text(&layout, "[F] Toggle Flashlight", HELP_COLOR);
	ui_layout_text(&layout, "[Z] Toggle Wireframe", HELP_COLOR);
	ui_layout_text(&layout, "[H] Toggle UI/Help", HELP_COLOR);
	ui_layout_text(&layout, "[J] Toggle Auto-Exposure", HELP_COLOR);
	ui_layout_text(&layout, "[B] Toggle Bloom", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: Environment */
	ui_layout_text(&layout, "--- Environment ---", HELP_COLOR);
	ui_layout_text(&layout, "[PgUp/PgDn] Change HDR", HELP_COLOR);
	ui_layout_text(&layout, "[Shift + PgUp/PgDn] Blur HDR", HELP_COLOR);

	ui_layout_separator(&layout, HELP_SECTION_PADDING);

	/* Section: Post-Process Styles */
	ui_layout_text(&layout, "--- Styles (Numpad) ---", HELP_COLOR);
	ui_layout_text(&layout, "[1] Default (Clean)", HELP_COLOR);
	ui_layout_text(&layout, "[2] Subtle", HELP_COLOR);
	ui_layout_text(&layout, "[3] Cinematic", HELP_COLOR);
	ui_layout_text(&layout, "[4] Vintage", HELP_COLOR);
	ui_layout_text(&layout, "[5] Matrix", HELP_COLOR);
	ui_layout_text(&layout, "[6] BW Contrast", HELP_COLOR);
	ui_layout_text(&layout, "[0] Reset", HELP_COLOR);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}

static void draw_exposure_debug_text(App* app)
{
	float exposure_val = 0.0F;
	glBindTexture(GL_TEXTURE_2D, app->postprocess.exposure_tex);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, &exposure_val);
	glBindTexture(GL_TEXTURE_2D, 0);

	char debug_text[DEBUG_TEXT_BUFFER_SIZE];
	float luminance =
	    (exposure_val > LUMINANCE_EPSILON) ? (1.0F / exposure_val) : 0.0F;
	(void)safe_snprintf(debug_text, sizeof(debug_text),
	                    "Auto Exposure: %.4f | Scene "
	                    "Lum: %.4f",
	                    exposure_val, luminance);

	ui_draw_text(&app->ui, debug_text, DEFAULT_FONT_OFFSET_X,
	             DEFAULT_FONT_OFFSET_Y + DEBUG_TEXT_Y_OFFSET,
	             (float*)DEBUG_ORANGE_COLOR, app->width, app->height);
}

static int compute_luminance_histogram(App* app, int* buckets, int size,
                                       float* min_lum, float* max_lum)
{
	const int MAP_SIZE = 64;
	const int TOTAL_PIXELS = MAP_SIZE * MAP_SIZE;
	float* lum_data = malloc((size_t)TOTAL_PIXELS * sizeof(float));

	if (!lum_data) {
		return 0;
	}

	glBindTexture(GL_TEXTURE_2D, app->postprocess.lum_downsample_tex);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, lum_data);
	glBindTexture(GL_TEXTURE_2D, 0);

	for (int i = 0; i < size; i++) {
		buckets[i] = 0;
	}

	static const float HISTO_MIN_INIT = 1000.0F;
	static const float HISTO_MAX_INIT = -1000.0F;
	*min_lum = HISTO_MIN_INIT;
	*max_lum = HISTO_MAX_INIT;

	for (int i = 0; i < TOTAL_PIXELS; i++) {
		float val = lum_data[i];
		if (val < *min_lum) {
			*min_lum = val;
		}
		if (val > *max_lum) {
			*max_lum = val;
		}

		static const float RANGE_OFFSET = 5.0F;
		static const float RANGE_SCALE = 10.0F;
		float norm = (val + RANGE_OFFSET) / RANGE_SCALE;
		int idx = (int)(norm * (float)size);
		if (idx < 0) {
			idx = 0;
		}
		if (idx >= size) {
			idx = size - 1;
		}

		buckets[idx]++;
	}

	free(lum_data);
	return 1;
}

static void draw_luminance_histogram_graph(App* app, const int* buckets,
                                           int size, float min_lum,
                                           float max_lum)
{
	static const float GRAPH_POS_X = 20.0F;
	static const float GRAPH_POS_Y_OFF = 200.0F;
	static const float GRAPH_DIM_W = 300.0F;
	static const float GRAPH_DIM_H = 100.0F;

	float graph_x = GRAPH_POS_X;
	float graph_y = (float)app->height - GRAPH_POS_Y_OFF;
	float graph_w = GRAPH_DIM_W;
	float graph_h = GRAPH_DIM_H;
	float bar_w = graph_w / (float)size;

	/* Background */
	ui_draw_rect(&app->ui, graph_x, graph_y, graph_w, graph_h,
	             (vec3){0.0F, 0.0F, 0.0F}, app->width, app->height);

	/* Find peak for scaling */
	int max_bucket = 1;
	for (int i = 0; i < size; i++) {
		if (buckets[i] > max_bucket) {
			max_bucket = buckets[i];
		}
	}

	/* Draw Bars */
	for (int i = 0; i < size; i++) {
		float h_val = (float)buckets[i] / (float)max_bucket * graph_h;
		vec3 bar_col;
		glm_vec3_copy((float*)HISTO_BAR_COLOR_GREEN, bar_col);
		if (i < size / 2) {
			glm_vec3_copy((float*)HISTO_BAR_COLOR_BLUE, bar_col);
		} else {
			glm_vec3_copy((float*)HISTO_BAR_COLOR_RED, bar_col);
		}

		ui_draw_rect(&app->ui, graph_x + ((float)i * bar_w),
		             graph_y + (graph_h - h_val), bar_w, h_val, bar_col,
		             app->width, app->height);
	}

	/* Draw Range Info */
	char range_text[RANGE_TEXT_BUFFER_SIZE];
	(void)safe_snprintf(range_text, sizeof(range_text),
	                    "Log Lum Range: [%.2f, %.2f]", min_lum, max_lum);
	ui_draw_text(&app->ui, range_text, graph_x,
	             graph_y - GRAPH_TEXT_PADDING, (float*)GRAPH_TEXT_COLOR,
	             app->width, app->height);
}

static void app_draw_debug_overlay(App* app)
{
	/* Auto Exposure Debug Text */
	if (postprocess_is_enabled(&app->postprocess, POSTFX_EXPOSURE_DEBUG)) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_DEPTH_TEST);

		draw_exposure_debug_text(app);

		/* -----------------------
		   Luminance Histogram
		   ----------------------- */
		const int HISTO_SIZE = 64;
		int buckets[HISTO_SIZE];
		float min_lum = 0.0F;
		float max_lum = 0.0F;

		if (compute_luminance_histogram(app, buckets, HISTO_SIZE,
		                                &min_lum, &max_lum) != 0) {
			draw_luminance_histogram_graph(app, buckets, HISTO_SIZE,
			                               min_lum, max_lum);
		}

		/* Cleanup */
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
	}
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void app_render_ui(App* app)
{
	/* --- Draw Main Info Overlay --- */
	UILayout layout;
	/* Start slightly offset from top-left */
	ui_layout_init(&layout, &app->ui, DEFAULT_FONT_OFFSET_X,
	               DEFAULT_FONT_OFFSET_Y, DEFAULT_SPACING, app->width,
	               app->height);

	/* Conditional text overlay rendering based on text_overlay_mode */
	/* Mode 0: Off, Mode 1: FPS+Position, Mode 2: FPS+Position+Envmap, 
	 * Mode 3: FPS+Position+Envmap+Exposure */

	/* 1. FPS - shown in modes 1, 2, 3 */
	if (app->text_overlay_mode >= 1) {
		static const float MS_PER_SECOND = 1000.0F;
		char fps_text[MAX_FPS_TEXT_LENGTH];
		float current_fps = 0.0F;
		float frame_time_ms = 0.0F;

		if (app->fps_counter.average_frame_time > 0.0F) {
			current_fps =
			    1.0F / (float)app->fps_counter.average_frame_time;
			frame_time_ms =
			    (float)app->fps_counter.average_frame_time *
			    MS_PER_SECOND;
		}

		(void)safe_snprintf(fps_text, sizeof(fps_text),
		                    "FPS: %.1f (%.2f ms)", current_fps,
		                    frame_time_ms);

		ui_layout_text(&layout, fps_text, DEFAULT_FONT_COLOR);
	}

	/* 2. Position - shown in modes 1, 2, 3 */
	if (app->text_overlay_mode >= 1) {
		char pos_text[DEBUG_TEXT_BUFFER_SIZE];
		(void)safe_snprintf(pos_text, sizeof(pos_text), "Pos: %.1f, %.1f, %.1f",
		                    app->camera.position[0], app->camera.position[1],
		                    app->camera.position[2]);
		ui_layout_text(&layout, pos_text, DEFAULT_FONT_COLOR);
	}

	/* 3. Environment - shown in modes 2, 3 */
	if (app->text_overlay_mode >= 2 && app->hdr_count > 0 &&
	    app->current_hdr_index >= 0) {
		char env_text[ENV_TEXT_BUFFER_SIZE];
		(void)safe_snprintf(env_text, sizeof(env_text), "Env: %s",
		                    app->hdr_files[app->current_hdr_index]);
		ui_layout_text(&layout, env_text, ENV_TEXT_COLOR);
	}

	/* 4. Exposure - shown in mode 3 only */
	if (app->text_overlay_mode >= 3) {
		float exposure_val = 0.0F;
		if (postprocess_is_enabled(&app->postprocess,
		                           POSTFX_AUTO_EXPOSURE)) {
			/* Async Readback using PBO to avoid pipeline stall */
			glBindBuffer(GL_PIXEL_PACK_BUFFER, app->exposure_pbo);

			/* 1. Read PREVIOUS frame's data (if available) */
			float* ptr = (float*)glMapBuffer(GL_PIXEL_PACK_BUFFER,
			                                 GL_READ_ONLY);
			if (ptr) {
				app->current_exposure = *ptr;
				glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			}

			/* 2. Trigger async read for CURRENT frame */
			glBindTexture(GL_TEXTURE_2D,
			              app->postprocess.exposure_tex);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT,
			              0); /* Offset 0 */
			glBindTexture(GL_TEXTURE_2D, 0);

			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

			exposure_val = app->current_exposure;
		} else {
			exposure_val = app->postprocess.exposure.exposure;
		}

		char exposure_text[EXPOSURE_TEXT_BUFFER_SIZE];
		(void)safe_snprintf(exposure_text, sizeof(exposure_text),
		                    "Exposure: %.3f", exposure_val);

		ui_layout_text(&layout, exposure_text, ENV_TEXT_COLOR);
	}

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	/* Auto Exposure Debug Text */
	if (postprocess_is_enabled(&app->postprocess, POSTFX_EXPOSURE_DEBUG)) {
		app_draw_debug_overlay(app);
	}

	/* Help Screen Overlay */
	if (app->show_help) {
		app_draw_help_overlay(app);
	}
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
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
			         postprocess_is_enabled(&app->postprocess,
			                                POSTFX_GRAIN)
			             ? "ON"
			             : "OFF");
			break;

		case GLFW_KEY_B: /* Toggle Bloom */
			postprocess_toggle(&app->postprocess, POSTFX_BLOOM);
			LOG_INFO("suckless-ogl.app", "Bloom: %s",
			         postprocess_is_enabled(&app->postprocess,
			                                POSTFX_BLOOM)
			             ? "ON"
			             : "OFF");
			break;

		case GLFW_KEY_H: /* Toggle DOF / Debug */
			if (glfwGetKey(app->window, GLFW_KEY_LEFT_SHIFT) ==
			        GLFW_PRESS ||
			    glfwGetKey(app->window, GLFW_KEY_RIGHT_SHIFT) ==
			        GLFW_PRESS) {
				postprocess_toggle(&app->postprocess,
				                   POSTFX_DOF_DEBUG);
				LOG_INFO(
				    "suckless-ogl.app", "DOF DEBUG: %s",
				    postprocess_is_enabled(&app->postprocess,
				                           POSTFX_DOF_DEBUG)
				        ? "ON"
				        : "OFF");
			} else {
				postprocess_toggle(&app->postprocess,
				                   POSTFX_DOF);
				LOG_INFO("suckless-ogl.app", "DOF: %s",
				         postprocess_is_enabled(
				             &app->postprocess, POSTFX_DOF)
				             ? "ON"
				             : "OFF");
			}
			break;

		case GLFW_KEY_X: /* Toggle Chromatic
		                    Aberration */
			postprocess_toggle(&app->postprocess,
			                   POSTFX_CHROM_ABBR);
			LOG_INFO("suckless-ogl.app", "Chromatic Aberration: %s",
			         postprocess_is_enabled(&app->postprocess,
			                                POSTFX_CHROM_ABBR)
			             ? "ON"
			             : "OFF");
			break;

		case GLFW_KEY_R: /* Reload Shaders */
			/* TODO: Implement shader reloading
			 * system */
			LOG_INFO("suckless-ogl.app",
			         "Shader reloading not "
			         "implemented yet");
			break;
		case GLFW_KEY_KP_ADD: /* Augmenter
		                         l'exposition */
		{
			float current = app->postprocess.exposure.exposure;
			postprocess_set_exposure(
			    &app->postprocess, current + DEFAULT_EXPOSURE_STEP);
			LOG_INFO("suckless-ogl.app", "Exposure: %.2f",
			         app->postprocess.exposure.exposure);
		} break;

		case GLFW_KEY_KP_SUBTRACT: /* Diminuer
		                              l'exposition
		                            */
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

		case GLFW_KEY_J: /* Toggle Auto Exposure */
			if (glfwGetKey(app->window, GLFW_KEY_LEFT_SHIFT) ==
			        GLFW_PRESS ||
			    glfwGetKey(app->window, GLFW_KEY_RIGHT_SHIFT) ==
			        GLFW_PRESS) {
				postprocess_toggle(&app->postprocess,
				                   POSTFX_EXPOSURE_DEBUG);
				LOG_INFO("suckless-ogl.app",
				         "Auto Exposure Debug: "
				         "%s",
				         postprocess_is_enabled(
				             &app->postprocess,
				             POSTFX_EXPOSURE_DEBUG)
				             ? "ON"
				             : "OFF");
			} else {
				postprocess_toggle(&app->postprocess,
				                   POSTFX_AUTO_EXPOSURE);
				LOG_INFO(
				    "suckless-ogl.app", "Auto Exposure: %s",
				    postprocess_is_enabled(&app->postprocess,
				                           POSTFX_AUTO_EXPOSURE)
				        ? "ON"
				        : "OFF");
			}
			break;

		case GLFW_KEY_F5: /* Cycle PBR Debug Modes
		                   */
		{
			app->pbr_debug_mode = (app->pbr_debug_mode + 1) %
			                      PBR_DEBUG_MODE_COUNT; /* 0..8
			                                             */
			const char* modeNames[] = {"Final PBR",
			                           "Albedo",
			                           "Normal",
			                           "Metallic",
			                           "Roughness",
			                           "AO",
			                           "Irradiance (Diff)",
			                           "Prefilter (Spec)",
			                           "BRDF LUT"};
			LOG_INFO("suckless-ogl.app", "PBR Debug Mode: %s",
			         modeNames[app->pbr_debug_mode]);
		} break;

		/* Presets pour le post-processing */
		case GLFW_KEY_1: /* Preset: Aucun */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_DEFAULT);
			postprocess_set_exposure(&app->postprocess,
			                         app->auto_threshold);
			LOG_INFO("suckless-ogl.app",
			         "Style: Aucun (rendu "
			         "pur) - Exposure: %.2f",
			         app->auto_threshold);
			break;

		case GLFW_KEY_2: /* Preset: Subtle */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_SUBTLE);
			LOG_INFO("suckless-ogl.app", "Style: Subtle");
			break;

		case GLFW_KEY_3: /* Preset: Cinématique */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_CINEMATIC);
			LOG_INFO("suckless-ogl.app", "Style: Cinématique");
			break;

		case GLFW_KEY_4: /* Preset: Vintage */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_VINTAGE);
			LOG_INFO("suckless-ogl.app", "Style: Vintage");
			break;

		case GLFW_KEY_5: /* Style: "Matrix" */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_MATRIX);
			LOG_INFO("suckless-ogl.app", "Style: Matrix Grading");
			break;

		case GLFW_KEY_6: /* Style: "Noir et Blanc
		                    Contrasté" */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_BW_CONTRAST);
			LOG_INFO("suckless-ogl.app", "Style: Noir & Blanc");
			break;

		case GLFW_KEY_0:
		case GLFW_KEY_KP_0:
			/* Reset complet */
			postprocess_apply_preset(&app->postprocess,
			                         &PRESET_DEFAULT);
			postprocess_set_exposure(&app->postprocess,
			                         app->auto_threshold);
			LOG_INFO("suckless-ogl.app",
			         "Color Grading: Reset to "
			         "Defaults");
			break;

		default:
			break;
	}
}

static void app_handle_env_input(App* app, int action, int mods, int key)
{
	if (action != GLFW_PRESS && action != GLFW_REPEAT) {
		return;
	}

	if (key == GLFW_KEY_PAGE_UP) {
		if (check_flag(mods, GLFW_MOD_SHIFT)) {
			app->env_lod += LOD_STEP;
			if (app->env_lod > MAX_ENV_LOD) {
				app->env_lod = MAX_ENV_LOD;
			}
			LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
			         app->env_lod);
		} else if (app->hdr_count > 1) {
			/* Next Environment */
			app->current_hdr_index =
			    (app->current_hdr_index + 1) % app->hdr_count;
			app_load_env_map(
			    app, app->hdr_files[app->current_hdr_index]);
		}
	} else if (key == GLFW_KEY_PAGE_DOWN) {
		if (check_flag(mods, GLFW_MOD_SHIFT)) {
			app->env_lod -= LOD_STEP;
			if (app->env_lod < MIN_ENV_LOD) {
				app->env_lod = MIN_ENV_LOD;
			}
			LOG_INFO("suckless-ogl.app", "Env LOD: %.1F",
			         app->env_lod);
		} else if (app->hdr_count > 1) {
			/* Previous Environment */
			app->current_hdr_index--;
			if (app->current_hdr_index < 0) {
				app->current_hdr_index = app->hdr_count - 1;
			}
			app_load_env_map(
			    app, app->hdr_files[app->current_hdr_index]);
		}
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
			case GLFW_KEY_F1: /* Cycle Text Overlays */
				app->text_overlay_mode =
				    (app->text_overlay_mode + 1) % 4;
				{
					const char* mode_names[] = {
					    "Off", "FPS + Position", "FPS + Position + Envmap",
					    "FPS + Position + Envmap + Exposure"};
					LOG_INFO(
					    "suckless-ogl.app",
					    "Text Overlay: %s",
					    mode_names[app->text_overlay_mode]);
				}
				break;
			case GLFW_KEY_F2: /* Toggle Help */
				app->show_help = !app->show_help;
				break;
			case GLFW_KEY_P:  // Handle 'P' for
			                  // Screenshot/Capture
				app_save_raw_frame(app, "capture_frame.raw");
				break;
			case GLFW_KEY_Z:  // key 'W' on
			                  // French layout
			                  // keyboard
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
				/* Reset camera to default
				 * position */
				camera_init(
				    &app->camera, DEFAULT_CAMERA_DISTANCE,
				    DEFAULT_CAMERA_YAW, DEFAULT_CAMERA_PITCH);
				app->env_lod = DEFAULT_ENV_LOD;
				LOG_INFO("suckless-ogl.app",
				         "Camera and LOD "
				         "reset");
				break;
			case GLFW_KEY_PAGE_UP:
			case GLFW_KEY_PAGE_DOWN:
				app_handle_env_input(app, action, mods, key);
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

	/* Update camera rotation (note: -delta_x for
	 * natural mouse movement) */
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
		          "Failed to allocate memory for "
		          "RAW capture");
		return;
	}

	// Use 1-byte alignment to handle any window
	// resolution correctly
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	FILE* file = fopen(filename, "wb");
	if (file) {
		size_t result = fwrite(pixels, 1, size, file);
		if (result != size) {
			LOG_ERROR("suckless-ogl.app",
			          "Failed to write RAW "
			          "frame to file: %s",
			          filename);
			return;
		}
		result = fclose(file);
		if (result != 0) {
			LOG_ERROR("suckless-ogl.app",
			          "Failed to close file "
			          "for RAW capture: %s",
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