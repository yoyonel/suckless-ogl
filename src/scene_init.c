#include "app.h"
#include "app_settings.h"
#include "billboard_rendering.h"
#include "ibl_coordinator.h"
#include "instanced_rendering.h"
#include "light_probes.h"
#include "log.h"
#include "material.h"
#include "platform/platform_fs.h"
#include "platform/platform_utils.h"
#include "render_utils.h"
#include "scene.h"
#include "scene_gpu_resources.h"
#include "scene_internal.h"
#include "scene_shaders.h"
#include "scene_simulation.h"
#include "scene_visuals.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char* const HDR_TEXTURE_PATH = "assets/textures/hdr";
static const char* const HDR_EXTENSION = ".hdr";

static int compare_strings(const void* string_a, const void* string_b)
{
	return strcmp(*(const char**)string_a, *(const char**)string_b);
}

struct HdrScanContext {
	Scene* scene;
};

static void scene_hdr_file_callback(const char* filename, bool is_dir,
                                    void* user_data)
{
	if (is_dir) {
		return;
	}

	struct HdrScanContext* ctx = (struct HdrScanContext*)user_data;
	Scene* scene = ctx->scene;

	const char* dot = strrchr(filename, '.');
	if (!dot || strcmp(dot, HDR_EXTENSION) != 0) {
		return;
	}

	size_t new_count = (size_t)scene->hdr_count + 1;
	char** new_files = realloc(scene->hdr_files, new_count * sizeof(char*));

	if (!new_files) {
		LOG_ERROR("suckless-ogl.scene",
		          "Failed to realloc memory for HDR files");
		return;
	}

	scene->hdr_files = new_files;
	scene->hdr_count++;
	scene->hdr_files[scene->hdr_count - 1] = strdup(filename);
}

static void scene_scan_hdr_files(Scene* scene)
{
	scene->hdr_count = 0;
	scene->hdr_files = NULL;
	scene->current_hdr_index = -1;

	struct HdrScanContext ctx = {scene};
	if (!platform_dir_list(HDR_TEXTURE_PATH, scene_hdr_file_callback,
	                       &ctx)) {
		LOG_ERROR("suckless-ogl.scene",
		          "Failed to open assets/textures/hdr directory!");
		return;
	}

	if (scene->hdr_count > 1) {
		qsort(scene->hdr_files, (size_t)scene->hdr_count, sizeof(char*),
		      compare_strings);
	}
	LOG_INFO("suckless-ogl.scene", "Found %d HDR files.", scene->hdr_count);
}

void scene_init_instancing(Scene* scene)
{
	const int total_count = MIN(scene->lighting.material_lib->count,
	                            DEFAULT_COLS * DEFAULT_COLS);
	const int cols = DEFAULT_COLS;
	const int rows = (total_count + cols - 1) / cols;
	const float spacing = DEFAULT_SPACING;

	const float grid_w = (float)(cols - 1) * spacing;
	const float grid_h = (float)(rows - 1) * spacing;

	SphereInstance* data = (SphereInstance*)platform_aligned_alloc(
	    sizeof(SphereInstance) * (size_t)total_count, SIMD_ALIGNMENT);
	if (!data) {
		LOG_ERROR("suckless-ogl.scene",
		          "Failed to allocate aligned memory for instancing");
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
		PBRMaterial* mat = &scene->lighting.material_lib->materials[i];
		glm_vec3_copy(mat->albedo, data[i].albedo);
		data[i].metallic = mat->metallic;
		data[i].roughness = mat->roughness;
		data[i].ao = 1.0F;
		/* Static grid: prev_center = current center (no object motion)
		 */
		glm_vec3_copy(position, data[i].prev_center);
	}

	instanced_group_init(&scene->instanced_group, data, total_count);

#ifdef USE_TRANSPARENT_BILLBOARDS
	// Use platform_aligned_alloc for portability and consistency
	void* raw_mem = platform_aligned_alloc(
	    sizeof(SphereInstance) * (size_t)total_count, SIMD_ALIGNMENT);
	if (raw_mem) {
		scene->billboard_instances = (SphereInstance*)raw_mem;
		safe_memcpy(scene->billboard_instances,
		            sizeof(SphereInstance) * (size_t)total_count, data,
		            sizeof(SphereInstance) * (size_t)total_count);
		scene->billboard_instance_count = total_count;
		billboard_sorter_init(&scene->billboard_sorter, total_count);
	}
#endif

	instanced_group_bind_mesh(
	    &scene->instanced_group, scene->gpu->icosphere_vbo,
	    scene->gpu->icosphere_nbo, scene->gpu->icosphere_ebo);
	billboard_group_init(&scene->billboard_group, data, total_count);
	billboard_group_prepare(&scene->billboard_group, scene->gpu->quad_vbo,
	                        scene->gpu->wire_quad_vbo,
	                        scene->gpu->wire_cube_vbo);

	/* Initialize Light Probe Grid with Scene Data */
	light_probe_grid_set_scene(&scene->lighting.probe_grid, data,
	                           total_count, sizeof(SphereInstance));

	/* Initialize Light Probe Grid Bounding Box with Scene Data */
	light_probe_grid_compute_aabb(&scene->lighting.probe_grid, data,
	                              total_count, sizeof(SphereInstance),
	                              DEFAULT_SPACING * HALF_OFFSET_MULTIPLIER);
	/* Trigger initial async calculation */
	light_probe_grid_update_async(&scene->lighting.probe_grid);

	free(data);
}

#ifdef USE_SSBO_RENDERING
static void scene_init_ssbo(Scene* scene)
{
	const int total_count = MIN(scene->lighting.material_lib->count,
	                            DEFAULT_COLS * DEFAULT_COLS);
	const int cols = DEFAULT_COLS;
	const int rows = (total_count + cols - 1) / cols;
	const float spacing = DEFAULT_SPACING;

	const float grid_w = (float)(cols - 1) * spacing;
	const float grid_h = (float)(rows - 1) * spacing;

	SphereInstanceSSBO* data =
	    malloc(sizeof(SphereInstanceSSBO) * (size_t)total_count);
	if (!data) {
		LOG_ERROR("suckless-ogl.scene",
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
		PBRMaterial* mat = &scene->lighting.material_lib->materials[i];
		glm_vec3_copy(mat->albedo, data[i].albedo);
		data[i].metallic = mat->metallic;
		data[i].roughness = mat->roughness;
		data[i].ao = 1.0F;
		data[i]._padding[0] = 0.0F;
		data[i]._padding[1] = 0.0F;
	}

	ssbo_group_init(&scene->ssbo_group, data, total_count);
	ssbo_group_bind_mesh(&scene->ssbo_group, scene->gpu->icosphere_vbo,
	                     scene->gpu->icosphere_nbo,
	                     scene->gpu->icosphere_ebo);

	/* Initialize Light Probe Grid with Scene Data (SSBO Mode) */
	light_probe_grid_set_scene(&scene->lighting.probe_grid, data,
	                           total_count, sizeof(SphereInstanceSSBO));

	/* Initialize Light Probe Grid Bounding Box with Scene Data */
	light_probe_grid_compute_aabb(&scene->lighting.probe_grid, data,
	                              total_count, sizeof(SphereInstanceSSBO),
	                              DEFAULT_SPACING * HALF_OFFSET_MULTIPLIER);
	light_probe_grid_update_async(&scene->lighting.probe_grid);

	free(data);
}
#endif

static void scene_init_state(Scene* scene)
{
	scene->config.subdivisions = INITIAL_SUBDIVISIONS;
	scene->config.wireframe = false;
	scene->config.env_lod = DEFAULT_ENV_LOD;
	scene->config.pbr_debug_mode = 0;
	scene->config.show_envmap = true;
	scene->config.billboard_mode = true;
	scene->config.specular_aa_enabled = DEFAULT_SPECULAR_AA_ENABLED;
	scene->config.aa_mode = AA_MODE_CURVATURE;
	scene->config.sorting_mode = SORTING_MODE_GPU_BITONIC;
	scene->config.gi_mode = GI_MODE_OFF;
	scene->config.show_probe_grid = false;
	scene->gpu->billboard_ubo_ptr = NULL;

	scene->billboard_sorter = (BillboardSorter){0};

	scene->gpu->dummy_black_tex =
	    render_utils_create_color_texture(0.0F, 0.0F, 0.0F, 0.0F);
	scene->gpu->dummy_white_tex =
	    render_utils_create_color_texture(1.0F, 1.0F, 1.0F, 1.0F);

	scene->gpu->brdf_lut_tex = build_brdf_lut_map(BRDF_LUT_MAP_SIZE);
	scene->gpu->hdr_texture = 0;
	scene->gpu->recycled_hdr_tex = 0;
	scene->gpu->spec_prefiltered_tex = 0;
	scene->gpu->irradiance_tex = 0;
	scene->gpu->transition_snapshot_tex = 0;

	for (int i = 0; i < IBL_TEXTURE_COUNT; i++) {
		scene->gpu->bound_ibl_textures[i] = 0;
	}

	scene_scan_hdr_files(scene);
	// Initial load of default map is handled by app or caller
	// We just setup the state here.
}

static int scene_init_core_shaders(Scene* scene)
{
	scene->shaders->skybox =
	    shader_load("shaders/background.vert", "shaders/background.frag");
	if (!scene->shaders->skybox) {
		return 0;
	}

	scene->shaders->debug =
	    shader_load("shaders/debug_tex.vert", "shaders/debug_tex.frag");
	if (!scene->shaders->debug) {
		return 0;
	}

	scene->shaders->debug_line =
	    shader_load("shaders/debug_line.vert", "shaders/debug_line.frag");
	if (!scene->shaders->debug_line) {
		return 0;
	}
	return 1;
}

static int scene_init_billboard_shader(Scene* scene)
{
	scene->shaders->pbr_billboard = shader_load(
	    "shaders/pbr_ibl_billboard.vert", "shaders/pbr_ibl_billboard.frag");
	if (!scene->shaders->pbr_billboard) {
		return 0;
	}

	/* Create UBO for billboard per-frame uniforms (binding = 1) */
	glGenBuffers(1, &scene->gpu->billboard_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, scene->gpu->billboard_ubo);
	GLbitfield flags = (GLbitfield)GL_MAP_WRITE_BIT |
	                   (GLbitfield)GL_MAP_PERSISTENT_BIT |
	                   (GLbitfield)GL_MAP_COHERENT_BIT;
	glBufferStorage(GL_UNIFORM_BUFFER, sizeof(BillboardUBO), NULL, flags);
	scene->gpu->billboard_ubo_ptr =
	    glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(BillboardUBO), flags);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, scene->gpu->billboard_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	/* Set Billboard SH Sampler Indices (units 8-14) */
	{
		shader_use(scene->shaders->pbr_billboard);
		for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
			enum { MAX_UNIFORM_NAME_LEN = 32 };

			char name[MAX_UNIFORM_NAME_LEN];
			(void)safe_snprintf(name, sizeof(name), "u_SHTexture%d",
			                    i);
			scene->shaders->billboard_uniforms.sh_textures[i] =
			    shader_get_uniform_location(
			        scene->shaders->pbr_billboard, name);
			if (scene->shaders->billboard_uniforms.sh_textures[i] !=
			    -1) {
				glUniform1i(scene->shaders->billboard_uniforms
				                .sh_textures[i],
				            TEXTURE_UNIT_SH_START + i);
			}
		}
	}
	return 1;
}

static int scene_init_compute_resources(Scene* scene)
{
	scene->gpu->spmap_program =
	    shader_load_compute("shaders/IBL/spmap.glsl");
	scene->gpu->irmap_program =
	    shader_load_compute("shaders/IBL/irmap.glsl");
	scene->gpu->lum_pass1_program =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass1.glsl");
	scene->gpu->lum_pass2_program =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass2.glsl");

	if (!scene->gpu->spmap_program || !scene->gpu->irmap_program ||
	    !scene->gpu->lum_pass1_program || !scene->gpu->lum_pass2_program) {
		return 0;
	}

	ibl_coordinator_init(
	    &scene->lighting.ibl_coord, scene->gpu->spmap_program,
	    scene->gpu->irmap_program, scene->gpu->lum_pass1_program,
	    scene->gpu->lum_pass2_program);
	return 1;
}

static int scene_init_instanced_shader(Scene* scene, Shader** out_shader)
{
#ifdef USE_SSBO_RENDERING
	scene_init_ssbo(scene);
	scene->shaders->pbr_ssbo = shader_load(
	    "shaders/pbr_ibl_ssbo.vert", "shaders/pbr_ibl_instanced.frag");
	if (!scene->shaders->pbr_ssbo) {
		return 0;
	}
	*out_shader = scene->shaders->pbr_ssbo;
#else
	scene_init_instancing(scene);
	scene->shaders->pbr_instanced = shader_load(
	    "shaders/pbr_ibl_instanced.vert", "shaders/pbr_ibl_instanced.frag");
	if (!scene->shaders->pbr_instanced) {
		return 0;
	}
	*out_shader = scene->shaders->pbr_instanced;
#endif

	scene->shaders->instanced_uniforms.debug_mode =
	    shader_get_uniform_location(*out_shader, "debugMode");
	scene->shaders->instanced_uniforms.cam_pos =
	    shader_get_uniform_location(*out_shader, "camPos");
	scene->shaders->instanced_uniforms.projection =
	    shader_get_uniform_location(*out_shader, "projection");
	scene->shaders->instanced_uniforms.view =
	    shader_get_uniform_location(*out_shader, "view");
	scene->shaders->instanced_uniforms.previous_view_proj =
	    shader_get_uniform_location(*out_shader, "previousViewProj");
	scene->shaders->instanced_uniforms.u_specular_aa_enabled =
	    shader_get_uniform_location(*out_shader, "u_specularAAEnabled");
	scene->shaders->instanced_uniforms.u_aa_mode =
	    shader_get_uniform_location(*out_shader, "u_aaMode");

	/* Probe Grid Uniforms */
	scene->shaders->instanced_uniforms.probe_grid_min =
	    shader_get_uniform_location(*out_shader, "u_ProbeGridMin");
	scene->shaders->instanced_uniforms.probe_grid_max =
	    shader_get_uniform_location(*out_shader, "u_ProbeGridMax");
	scene->shaders->instanced_uniforms.probe_grid_dim =
	    shader_get_uniform_location(*out_shader, "u_ProbeGridDim");
	scene->shaders->instanced_uniforms.gi_mode =
	    shader_get_uniform_location(*out_shader, "u_GIMode");

	/* Set Instanced SH Sampler Indices (units 8-14) */
	{
		shader_use(*out_shader);
		for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
			enum { MAX_UNIFORM_NAME_LEN = 32 };

			char name[MAX_UNIFORM_NAME_LEN];
			(void)safe_snprintf(name, sizeof(name), "u_SHTexture%d",
			                    i);
			scene->shaders->instanced_uniforms.sh_textures[i] =
			    shader_get_uniform_location(*out_shader, name);
			if (scene->shaders->instanced_uniforms.sh_textures[i] !=
			    -1) {
				glUniform1i(scene->shaders->instanced_uniforms
				                .sh_textures[i],
				            TEXTURE_UNIT_SH_START + i);
			}
		}
	}
	return 1;
}

int scene_init(Scene* scene)
{
	/* Allocate opaque sub-structs */
	scene->gpu = calloc(1, sizeof(SceneGPUResources));
	scene->shaders = calloc(1, sizeof(SceneShaders));
	scene->simulation = calloc(1, sizeof(SceneSimulation));
	scene->visuals = calloc(1, sizeof(SceneVisuals));
	if (!scene->gpu || !scene->shaders || !scene->simulation ||
	    !scene->visuals) {
		LOG_ERROR("Scene", "Failed to allocate scene sub-structs");
		return 0;
	}

	scene_init_state(scene);

	if (!scene_init_core_shaders(scene)) {
		return 0;
	}

	render_utils_create_empty_vao(&scene->gpu->empty_vao);

	if (!scene_init_billboard_shader(scene)) {
		return 0;
	}

	render_utils_create_quad_vbo(&scene->gpu->quad_vbo);
	render_utils_create_wire_cube_vbo(&scene->gpu->wire_cube_vbo);
	render_utils_create_wire_quad_vbo(&scene->gpu->wire_quad_vbo);
	skybox_init(&scene->visuals->skybox, scene->shaders->skybox);
	icosphere_init(&scene->geometry);

	glGenVertexArrays(1, &scene->gpu->icosphere_vao);
	glGenBuffers(1, &scene->gpu->icosphere_vbo);
	glGenBuffers(1, &scene->gpu->icosphere_nbo);
	glGenBuffers(1, &scene->gpu->icosphere_ebo);

	scene->lighting.material_lib =
	    material_load_presets("assets/materials/pbr_materials.json");

	if (!scene_init_compute_resources(scene)) {
		return 0;
	}

	/* Initialize Probe Grid: dense grid covering the full sphere extent. */
	{
		int sphere_count = scene->lighting.material_lib->count;
		if (sphere_count > (DEFAULT_COLS * DEFAULT_COLS)) {
			sphere_count = DEFAULT_COLS * DEFAULT_COLS;
		}
		const int pcols = DEFAULT_COLS;
		const int prows = (sphere_count + pcols - 1) / pcols;
		light_probe_grid_init(&scene->lighting.probe_grid,
		                      (2 * pcols) + 1, (2 * prows) + 1, 3);
	}

	Shader* inst_shader = NULL;
	if (!scene_init_instanced_shader(scene, &inst_shader)) {
		return 0;
	}

	scene->shaders->debug_uniforms.projection = shader_get_uniform_location(
	    scene->shaders->debug_line, "projection");
	scene->shaders->debug_uniforms.view =
	    shader_get_uniform_location(scene->shaders->debug_line, "view");
	scene->shaders->debug_uniforms.u_stippled = shader_get_uniform_location(
	    scene->shaders->debug_line, "u_stippled");
	scene->shaders->debug_uniforms.u_billboard_mode =
	    shader_get_uniform_location(scene->shaders->debug_line,
	                                "u_billboardMode");
	scene->shaders->debug_uniforms.u_use_instance_col =
	    shader_get_uniform_location(scene->shaders->debug_line,
	                                "u_useInstanceColor");
	scene->shaders->debug_uniforms.u_color =
	    shader_get_uniform_location(scene->shaders->debug_line, "u_color");

	return 1;
}

/* --- Subsystem descriptor (Phase 1 alloc + Phase 3 GL init) --- */

int scene_subsys_init(App* app)
{
	app->scene =
	    platform_aligned_alloc(sizeof(*app->scene), SIMD_ALIGNMENT);
	if (!app->scene) {
		return 0;
	}
	*app->scene = (Scene){0};
	app->scene->config.specular_aa_enabled = DEFAULT_SPECULAR_AA_ENABLED;
	if (!scene_init(app->scene)) {
		scene_cleanup(app->scene);
		platform_aligned_free(app->scene);
		app->scene = NULL;
		return 0;
	}
	return 1;
}

void scene_subsys_cleanup(App* app)
{
	if (app->scene) {
		scene_cleanup(app->scene);
		platform_aligned_free(app->scene);
		app->scene = NULL;
	}
}
