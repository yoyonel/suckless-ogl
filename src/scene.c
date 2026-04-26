#include "scene.h"

#include "app_settings.h"
#include "billboard_rendering.h"
#include "billboard_sorting.h"
#include "gl_debug.h"
#include "glad/glad.h"
#include "ibl_coordinator.h"
#include "icosphere.h"
#include "instanced_rendering.h"
#include "log.h"
#include "material.h"
#include "platform/platform_fs.h"
#include "platform/platform_utils.h"
#include "profiler.h"
#include "render_utils.h"
#include "shader.h"
#include "utils.h"
#include <stdio.h>
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

static void scene_init_instancing(Scene* scene)
{
	const int total_count =
	    MIN(scene->material_lib->count, DEFAULT_COLS * DEFAULT_COLS);
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
		// NOLINTNEXTLINE(misc-include-cleaner)
		glm_translate(data[i].model, position);
		PBRMaterial* mat = &scene->material_lib->materials[i];
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

	instanced_group_bind_mesh(&scene->instanced_group, scene->icosphere_vbo,
	                          scene->icosphere_nbo, scene->icosphere_ebo);
	billboard_group_init(&scene->billboard_group, data, total_count);
	billboard_group_prepare(&scene->billboard_group, scene->quad_vbo,
	                        scene->wire_quad_vbo, scene->wire_cube_vbo);

	/* Initialize Light Probe Grid with Scene Data */
	light_probe_grid_set_scene(&scene->probe_grid, data, total_count,
	                           sizeof(SphereInstance));

	/* Initialize Light Probe Grid Bounding Box with Scene Data */
	light_probe_grid_compute_aabb(&scene->probe_grid, data, total_count,
	                              sizeof(SphereInstance),
	                              DEFAULT_SPACING * HALF_OFFSET_MULTIPLIER);
	/* Trigger initial async calculation */
	light_probe_grid_update_async(&scene->probe_grid);

	free(data);
}

#ifdef USE_SSBO_RENDERING
static void scene_init_ssbo(Scene* scene)
{
	const int total_count =
	    MIN(scene->material_lib->count, DEFAULT_COLS * DEFAULT_COLS);
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
		PBRMaterial* mat = &scene->material_lib->materials[i];
		glm_vec3_copy(mat->albedo, data[i].albedo);
		data[i].metallic = mat->metallic;
		data[i].roughness = mat->roughness;
		data[i].ao = 1.0F;
		data[i]._padding[0] = 0.0F;
		data[i]._padding[1] = 0.0F;
	}

	ssbo_group_init(&scene->ssbo_group, data, total_count);
	ssbo_group_bind_mesh(&scene->ssbo_group, scene->icosphere_vbo,
	                     scene->icosphere_nbo, scene->icosphere_ebo);

	/* Initialize Light Probe Grid with Scene Data (SSBO Mode) */
	light_probe_grid_set_scene(&scene->probe_grid, data, total_count,
	                           sizeof(SphereInstanceSSBO));

	/* Initialize Light Probe Grid Bounding Box with Scene Data */
	light_probe_grid_compute_aabb(&scene->probe_grid, data, total_count,
	                              sizeof(SphereInstanceSSBO),
	                              DEFAULT_SPACING * HALF_OFFSET_MULTIPLIER);
	light_probe_grid_update_async(&scene->probe_grid);

	free(data);
}
#endif

static void scene_init_state(Scene* scene)
{
	scene->subdivisions = INITIAL_SUBDIVISIONS;
	scene->wireframe = 0;
	scene->env_lod = DEFAULT_ENV_LOD;
	scene->pbr_debug_mode = 0;
	scene->show_envmap = 1;
	scene->billboard_mode = 1;
	scene->specular_aa_enabled = DEFAULT_SPECULAR_AA_ENABLED;
	scene->aa_mode = AA_MODE_CURVATURE;
	scene->sorting_mode = SORTING_MODE_GPU_BITONIC;
	scene->gi_mode = GI_MODE_OFF;
	scene->show_probe_grid = 0;
	scene->billboard_ubo_ptr = NULL;

	// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
	memset(&scene->billboard_sorter, 0, sizeof(BillboardSorter));

	scene->dummy_black_tex =
	    render_utils_create_color_texture(0.0F, 0.0F, 0.0F, 0.0F);
	scene->dummy_white_tex =
	    render_utils_create_color_texture(1.0F, 1.0F, 1.0F, 1.0F);

	scene->brdf_lut_tex = build_brdf_lut_map(BRDF_LUT_MAP_SIZE);
	scene->hdr_texture = 0;
	scene->recycled_hdr_tex = 0;
	scene->spec_prefiltered_tex = 0;
	scene->irradiance_tex = 0;
	scene->transition_snapshot_tex = 0;

	for (int i = 0; i < IBL_TEXTURE_COUNT; i++) {
		scene->bound_ibl_textures[i] = 0;
	}

	scene_scan_hdr_files(scene);
	// Initial load of default map is handled by app or caller
	// We just setup the state here.
}

static int scene_init_core_shaders(Scene* scene)
{
	scene->skybox_shader =
	    shader_load("shaders/background.vert", "shaders/background.frag");
	if (!scene->skybox_shader) {
		return 0;
	}

	scene->debug_shader =
	    shader_load("shaders/debug_tex.vert", "shaders/debug_tex.frag");
	if (!scene->debug_shader) {
		return 0;
	}

	scene->debug_line_shader =
	    shader_load("shaders/debug_line.vert", "shaders/debug_line.frag");
	if (!scene->debug_line_shader) {
		return 0;
	}
	return 1;
}

static int scene_init_billboard_shader(Scene* scene)
{
	scene->pbr_billboard_shader = shader_load(
	    "shaders/pbr_ibl_billboard.vert", "shaders/pbr_ibl_billboard.frag");
	if (!scene->pbr_billboard_shader) {
		return 0;
	}

	/* Create UBO for billboard per-frame uniforms (binding = 1) */
	glGenBuffers(1, &scene->billboard_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, scene->billboard_ubo);
	GLbitfield flags = (GLbitfield)GL_MAP_WRITE_BIT |
	                   (GLbitfield)GL_MAP_PERSISTENT_BIT |
	                   (GLbitfield)GL_MAP_COHERENT_BIT;
	glBufferStorage(GL_UNIFORM_BUFFER, sizeof(BillboardUBO), NULL, flags);
	scene->billboard_ubo_ptr =
	    glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(BillboardUBO), flags);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, scene->billboard_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	/* Set Billboard SH Sampler Indices (units 8-14) */
	{
		shader_use(scene->pbr_billboard_shader);
		for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
			enum { MAX_UNIFORM_NAME_LEN = 32 };
			char name[MAX_UNIFORM_NAME_LEN];
			(void)safe_snprintf(name, sizeof(name), "u_SHTexture%d",
			                    i);
			scene->billboard_uniforms.sh_textures[i] =
			    shader_get_uniform_location(
			        scene->pbr_billboard_shader, name);
			if (scene->billboard_uniforms.sh_textures[i] != -1) {
				glUniform1i(
				    scene->billboard_uniforms.sh_textures[i],
				    TEXTURE_UNIT_SH_START + i);
			}
		}
	}
	return 1;
}

static int scene_init_compute_resources(Scene* scene)
{
	scene->shader_spmap = shader_load_compute("shaders/IBL/spmap.glsl");
	scene->shader_irmap = shader_load_compute("shaders/IBL/irmap.glsl");
	scene->shader_lum_pass1 =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass1.glsl");
	scene->shader_lum_pass2 =
	    shader_load_compute("shaders/IBL/luminance_reduce_pass2.glsl");

	if (!scene->shader_spmap || !scene->shader_irmap ||
	    !scene->shader_lum_pass1 || !scene->shader_lum_pass2) {
		return 0;
	}

	ibl_coordinator_init(&scene->ibl_coord, scene->shader_spmap,
	                     scene->shader_irmap, scene->shader_lum_pass1,
	                     scene->shader_lum_pass2);
	return 1;
}

static int scene_init_instanced_shader(Scene* scene, Shader** out_shader)
{
#ifdef USE_SSBO_RENDERING
	scene_init_ssbo(scene);
	scene->pbr_ssbo_shader = shader_load("shaders/pbr_ibl_ssbo.vert",
	                                     "shaders/pbr_ibl_instanced.frag");
	if (!scene->pbr_ssbo_shader) {
		return 0;
	}
	*out_shader = scene->pbr_ssbo_shader;
#else
	scene_init_instancing(scene);
	scene->pbr_instanced_shader = shader_load(
	    "shaders/pbr_ibl_instanced.vert", "shaders/pbr_ibl_instanced.frag");
	if (!scene->pbr_instanced_shader) {
		return 0;
	}
	*out_shader = scene->pbr_instanced_shader;
#endif

	scene->instanced_uniforms.debug_mode =
	    shader_get_uniform_location(*out_shader, "debugMode");
	scene->instanced_uniforms.cam_pos =
	    shader_get_uniform_location(*out_shader, "camPos");
	scene->instanced_uniforms.projection =
	    shader_get_uniform_location(*out_shader, "projection");
	scene->instanced_uniforms.view =
	    shader_get_uniform_location(*out_shader, "view");
	scene->instanced_uniforms.previous_view_proj =
	    shader_get_uniform_location(*out_shader, "previousViewProj");
	scene->instanced_uniforms.u_specular_aa_enabled =
	    shader_get_uniform_location(*out_shader, "u_specularAAEnabled");
	scene->instanced_uniforms.u_aa_mode =
	    shader_get_uniform_location(*out_shader, "u_aaMode");

	/* Probe Grid Uniforms */
	scene->instanced_uniforms.probe_grid_min =
	    shader_get_uniform_location(*out_shader, "u_ProbeGridMin");
	scene->instanced_uniforms.probe_grid_max =
	    shader_get_uniform_location(*out_shader, "u_ProbeGridMax");
	scene->instanced_uniforms.probe_grid_dim =
	    shader_get_uniform_location(*out_shader, "u_ProbeGridDim");
	scene->instanced_uniforms.gi_mode =
	    shader_get_uniform_location(*out_shader, "u_GIMode");

	/* Set Instanced SH Sampler Indices (units 8-14) */
	{
		shader_use(*out_shader);
		for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
			enum { MAX_UNIFORM_NAME_LEN = 32 };
			char name[MAX_UNIFORM_NAME_LEN];
			(void)safe_snprintf(name, sizeof(name), "u_SHTexture%d",
			                    i);
			scene->instanced_uniforms.sh_textures[i] =
			    shader_get_uniform_location(*out_shader, name);
			if (scene->instanced_uniforms.sh_textures[i] != -1) {
				glUniform1i(
				    scene->instanced_uniforms.sh_textures[i],
				    TEXTURE_UNIT_SH_START + i);
			}
		}
	}
	return 1;
}

int scene_init(Scene* scene)
{
	scene_init_state(scene);

	if (!scene_init_core_shaders(scene)) {
		return 0;
	}

	render_utils_create_empty_vao(&scene->empty_vao);

	if (!scene_init_billboard_shader(scene)) {
		return 0;
	}

	render_utils_create_quad_vbo(&scene->quad_vbo);
	render_utils_create_wire_cube_vbo(&scene->wire_cube_vbo);
	render_utils_create_wire_quad_vbo(&scene->wire_quad_vbo);
	skybox_init(&scene->skybox, scene->skybox_shader);
	icosphere_init(&scene->geometry);

	glGenVertexArrays(1, &scene->icosphere_vao);
	glGenBuffers(1, &scene->icosphere_vbo);
	glGenBuffers(1, &scene->icosphere_nbo);
	glGenBuffers(1, &scene->icosphere_ebo);

	scene->material_lib =
	    material_load_presets("assets/materials/pbr_materials.json");

	if (!scene_init_compute_resources(scene)) {
		return 0;
	}

	/* Initialize Probe Grid: dense grid covering the full sphere extent. */
	{
		int sphere_count = scene->material_lib->count;
		if (sphere_count > (DEFAULT_COLS * DEFAULT_COLS)) {
			sphere_count = DEFAULT_COLS * DEFAULT_COLS;
		}
		const int pcols = DEFAULT_COLS;
		const int prows = (sphere_count + pcols - 1) / pcols;
		light_probe_grid_init(&scene->probe_grid, (2 * pcols) + 1,
		                      (2 * prows) + 1, 3);
	}

	Shader* inst_shader = NULL;
	if (!scene_init_instanced_shader(scene, &inst_shader)) {
		return 0;
	}

	scene->debug_uniforms.projection =
	    shader_get_uniform_location(scene->debug_line_shader, "projection");
	scene->debug_uniforms.view =
	    shader_get_uniform_location(scene->debug_line_shader, "view");
	scene->debug_uniforms.u_stippled =
	    shader_get_uniform_location(scene->debug_line_shader, "u_stippled");
	scene->debug_uniforms.u_billboard_mode = shader_get_uniform_location(
	    scene->debug_line_shader, "u_billboardMode");
	scene->debug_uniforms.u_use_instance_col = shader_get_uniform_location(
	    scene->debug_line_shader, "u_useInstanceColor");
	scene->debug_uniforms.u_color =
	    shader_get_uniform_location(scene->debug_line_shader, "u_color");

	return 1;
}

static void scene_cleanup_pbr_shaders(Scene* scene)
{
	SHADER_SAFE_DESTROY(scene->pbr_instanced_shader);
	SHADER_SAFE_DESTROY(scene->pbr_billboard_shader);
#ifdef USE_SSBO_RENDERING
	SHADER_SAFE_DESTROY(scene->pbr_ssbo_shader);
#endif
	GL_SAFE_DELETE_PROGRAM(scene->shader_spmap);
	GL_SAFE_DELETE_PROGRAM(scene->shader_irmap);
}

static void scene_cleanup_shaders(Scene* scene)
{
	scene_cleanup_pbr_shaders(scene);

	SHADER_SAFE_DESTROY(scene->debug_shader);
	SHADER_SAFE_DESTROY(scene->debug_line_shader);
	SHADER_SAFE_DESTROY(scene->skybox_shader);
	GL_SAFE_DELETE_PROGRAM(scene->shader_lum_pass1);
	GL_SAFE_DELETE_PROGRAM(scene->shader_lum_pass2);
}

static void scene_cleanup_geometry_buffers(Scene* scene)
{
	GL_SAFE_DELETE_VAO(scene->icosphere_vao);
	GL_SAFE_DELETE_BUFFER(scene->icosphere_vbo);
	GL_SAFE_DELETE_BUFFER(scene->icosphere_nbo);
	GL_SAFE_DELETE_BUFFER(scene->icosphere_ebo);
}

static void scene_cleanup_buffers(Scene* scene)
{
	scene_cleanup_geometry_buffers(scene);

	GL_SAFE_DELETE_VAO(scene->empty_vao);
	GL_SAFE_DELETE_BUFFER(scene->wire_cube_vbo);
	GL_SAFE_DELETE_BUFFER(scene->wire_quad_vbo);
	GL_SAFE_DELETE_BUFFER(scene->quad_vbo);
	GL_SAFE_DELETE_BUFFERS(2, scene->lum_ssbo);
	GL_SAFE_DELETE_BUFFER(scene->billboard_ubo);
}

static void scene_cleanup_textures(Scene* scene)
{
	GL_SAFE_DELETE_TEXTURE(scene->hdr_texture);
	GL_SAFE_DELETE_TEXTURE(scene->recycled_hdr_tex);
	GL_SAFE_DELETE_TEXTURE(scene->brdf_lut_tex);
	GL_SAFE_DELETE_TEXTURE(scene->spec_prefiltered_tex);
	GL_SAFE_DELETE_TEXTURE(scene->irradiance_tex);
	GL_SAFE_DELETE_TEXTURE(scene->dummy_black_tex);
	GL_SAFE_DELETE_TEXTURE(scene->dummy_white_tex);
	GL_SAFE_DELETE_TEXTURE(scene->transition_snapshot_tex);
}

static void scene_cleanup_gpu_resources(Scene* scene)
{
	scene_cleanup_buffers(scene);
	scene_cleanup_textures(scene);
}

void scene_cleanup(Scene* scene)
{
	if (!scene) {
		return;
	}

	icosphere_free(&scene->geometry);
	skybox_cleanup(&scene->skybox);
#ifdef USE_TRANSPARENT_BILLBOARDS
	if (scene->billboard_instances) {
		platform_aligned_free(scene->billboard_instances);
		scene->billboard_instances = NULL;
	}
	billboard_sorter_cleanup(&scene->billboard_sorter);
#endif
	instanced_group_cleanup(&scene->instanced_group);
	billboard_group_cleanup(&scene->billboard_group);
	trail_renderer_cleanup(&scene->trail_renderer);
#ifdef USE_SSBO_RENDERING
	ssbo_group_cleanup(&scene->ssbo_group);
#endif

	if (scene->material_lib) {
		material_free_lib(scene->material_lib);
		scene->material_lib = NULL;
	}

	scene_cleanup_shaders(scene);
	scene_cleanup_gpu_resources(scene);

	ibl_coordinator_cleanup(&scene->ibl_coord);
	light_probe_grid_cleanup(&scene->probe_grid);

	if (scene->hdr_files) {
		for (int i = 0; i < scene->hdr_count; i++) {
			free(scene->hdr_files[i]);
			scene->hdr_files[i] = NULL;
		}
		free(scene->hdr_files);
		scene->hdr_files = NULL;
		scene->hdr_count = 0;
	}
}

const char* aa_mode_to_string(AAMode mode)
{
	switch (mode) {
		case AA_MODE_SCREEN_SPACE:
			return "Screen-space";
		case AA_MODE_CURVATURE:
			return "Curvature-based";
		default:
			return "Unknown";
	}
}

void scene_update_gpu_buffers(Scene* scene)
{
	glBindVertexArray(scene->icosphere_vao);
	glBindBuffer(GL_ARRAY_BUFFER, scene->icosphere_vbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(scene->geometry.vertices.size * sizeof(vec3)),
	             scene->geometry.vertices.data, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, scene->icosphere_nbo);
	glBufferData(GL_ARRAY_BUFFER,
	             (GLsizeiptr)(scene->geometry.normals.size * sizeof(vec3)),
	             scene->geometry.normals.data, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->icosphere_ebo);
	glBufferData(
	    GL_ELEMENT_ARRAY_BUFFER,
	    (GLsizeiptr)(scene->geometry.indices.size * sizeof(unsigned int)),
	    scene->geometry.indices.data, GL_STATIC_DRAW);
	glBindVertexArray(0);
}

/**
 * @brief Binds SH 3D textures (units 8-14) and probe SSBO only when changed.
 * Units 8-14 and SSBO binding 3 are exclusive to PBR passes — safe to cache.
 * Invalidated after light_probe_grid_sync() which clobbers GL_TEXTURE_3D.
 */
static void scene_bind_probe_textures(Scene* scene)
{
	for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
		GLuint tex = scene->probe_grid.sh_textures[i];
		if (tex != scene->bound_sh_textures[i]) {
			glActiveTexture(
			    (GLenum)(GL_TEXTURE0 + TEXTURE_UNIT_SH_START + i));
			glBindTexture(GL_TEXTURE_3D, tex);
			scene->bound_sh_textures[i] = tex;
		}
	}

	if (scene->probe_grid.ssbo != scene->bound_probe_ssbo) {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3,
		                 scene->probe_grid.ssbo);
		scene->bound_probe_ssbo = scene->probe_grid.ssbo;
	}
}

static void scene_bind_ibl_textures(Scene* scene)
{
	GLuint textures[IBL_TEXTURE_COUNT] = {
	    scene->irradiance_tex ? scene->irradiance_tex
	                          : scene->dummy_black_tex,
	    scene->spec_prefiltered_tex ? scene->spec_prefiltered_tex
	                                : scene->dummy_black_tex,
	    scene->brdf_lut_tex ? scene->brdf_lut_tex : scene->dummy_black_tex};

	for (int i = 0; i < IBL_TEXTURE_COUNT; i++) {
		if (textures[i] != scene->bound_ibl_textures[i]) {
			glActiveTexture(
			    (GLenum)(GL_TEXTURE0 + TEXTURE_UNIT_IBL_START + i));
			glBindTexture(GL_TEXTURE_2D, textures[i]);
			scene->bound_ibl_textures[i] = textures[i];
		}
	}
}

static void scene_render_billboards(Scene* scene, mat4 view, mat4 proj,
                                    vec3 camera_pos, mat4 previous_view_proj,
                                    int width, int height)
{
	Shader* current_shader = scene->pbr_billboard_shader;
	shader_use(current_shader);

	scene_bind_ibl_textures(scene);

	/* Upload all per-frame uniforms via UBO (binding = 1) */
	{
		BillboardUBO ubo = {0};
		glm_mat4_copy(proj, (vec4*)ubo.projection);
		glm_mat4_copy(view, (vec4*)ubo.view);
		glm_mat4_copy(previous_view_proj,
		              (vec4*)ubo.previous_view_proj);
		glm_vec3_copy(camera_pos, ubo.cam_pos);
		ubo.debug_mode = scene->pbr_debug_mode;
		ubo.screen_size[0] = (float)width;
		ubo.screen_size[1] = (float)height;
		glm_vec3_copy(scene->probe_grid.aabb_min, ubo.probe_grid_min);
		ubo.gi_mode = (int32_t)scene->gi_mode;
		glm_vec3_copy(scene->probe_grid.aabb_max, ubo.probe_grid_max);
		ubo.specular_aa_enabled = scene->specular_aa_enabled;
		ubo.probe_grid_dim[0] = scene->probe_grid.grid_dim[0];
		ubo.probe_grid_dim[1] = scene->probe_grid.grid_dim[1];
		ubo.probe_grid_dim[2] = scene->probe_grid.grid_dim[2];
		ubo.aa_mode = scene->aa_mode;

		if (scene->billboard_ubo_ptr) {
			// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
			memcpy(scene->billboard_ubo_ptr, &ubo,
			       sizeof(BillboardUBO));
		}
	}

	scene_bind_probe_textures(scene);

	/* Debug Visualization Constants */
	const float debug_fill_alpha = 0.10F;
	const float debug_box_alpha = 0.5F;
	const float debug_offset_fill = 1.0F;
	const float debug_offset_line = -2.0F;

	billboard_group_draw(&scene->billboard_group);

	if (scene->pbr_debug_mode == 0 && scene->wireframe) {
		/* Wireframe Overlay */
		/* Enable Depth Test but disable Depth Write to overlay
		 * correctly */
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		shader_use(scene->debug_line_shader);
		shader_set_mat4_loc(scene->debug_uniforms.projection,
		                    (float*)proj);
		shader_set_mat4_loc(scene->debug_uniforms.view, (float*)view);

		/* 0. Transparent Fill (Instance Albedo) */
		/* Push fill back to avoid z-fighting with outlines */
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(debug_offset_fill, debug_offset_fill);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		/* Disable stipple, Enable Billboard Mode, Enable Instance Color
		 */
		shader_set_int_loc(scene->debug_uniforms.u_stippled, 0);
		shader_set_int_loc(scene->debug_uniforms.u_billboard_mode, 1);
		shader_set_int_loc(scene->debug_uniforms.u_use_instance_col, 1);
		/* Alpha 0.10 for transparency */
		float color_fill[4] = {1.0F, 1.0F, 1.0F, debug_fill_alpha};
		shader_set_vec4_loc(scene->debug_uniforms.u_color, color_fill);
		billboard_group_draw_debug_fill(&scene->billboard_group);
		glDisable(GL_BLEND);
		glDisable(GL_POLYGON_OFFSET_FILL);

		/* 1. Quad Outline (Solid Green/White) */
		/* Pull outlines forward */
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset(debug_offset_line, debug_offset_line);

		/* Disable stipple, Enable Billboard Mode, Disable Instance
		 * Color */
		shader_set_int_loc(scene->debug_uniforms.u_stippled, 0);
		shader_set_int_loc(scene->debug_uniforms.u_billboard_mode, 1);
		shader_set_int_loc(scene->debug_uniforms.u_use_instance_col, 0);
		float color_quad[4] = {0.0F, 1.0F, 0.0F, 1.0F};
		shader_set_vec4_loc(scene->debug_uniforms.u_color, color_quad);
		billboard_group_draw_debug_quads(&scene->billboard_group);

		/* 2. Bounding Box (Dotted/Stippled Red/Yellow) */
		/* Enable stipple, Disable Billboard Mode */
		shader_set_int_loc(scene->debug_uniforms.u_stippled, 1);
		shader_set_int_loc(scene->debug_uniforms.u_billboard_mode, 0);
		float color_box[4] = {1.0F, 1.0F, 0.0F, debug_box_alpha};
		shader_set_vec4_loc(scene->debug_uniforms.u_color, color_box);
		billboard_group_draw_debug_boxes(&scene->billboard_group);

		glDisable(GL_POLYGON_OFFSET_LINE);

		/* Restore Depth State */
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
	}
}

static void scene_render_instanced(Scene* scene, mat4 view, mat4 proj,
                                   vec3 camera_pos, mat4 previous_view_proj)
{
	Shader* current_shader = NULL;
#ifdef USE_SSBO_RENDERING
	current_shader = scene->pbr_ssbo_shader;
#else
	current_shader = scene->pbr_instanced_shader;
#endif

	shader_use(current_shader);

	scene_bind_ibl_textures(scene);

	shader_set_int_loc(scene->instanced_uniforms.debug_mode,
	                   scene->pbr_debug_mode);
	shader_set_vec3_loc(scene->instanced_uniforms.cam_pos, camera_pos);
	shader_set_mat4_loc(scene->instanced_uniforms.projection, (float*)proj);
	shader_set_mat4_loc(scene->instanced_uniforms.view, (float*)view);
	shader_set_mat4_loc(scene->instanced_uniforms.previous_view_proj,
	                    (float*)previous_view_proj);

	if (scene->instanced_uniforms.u_specular_aa_enabled != -1) {
		glUniform1i(scene->instanced_uniforms.u_specular_aa_enabled,
		            scene->specular_aa_enabled);
	}
	if (scene->instanced_uniforms.u_aa_mode != -1) {
		glUniform1i(scene->instanced_uniforms.u_aa_mode,
		            scene->aa_mode);
	}

	/* Probe Grid spatial bounds and GI Toggle */
	shader_set_vec3_loc(scene->instanced_uniforms.probe_grid_min,
	                    scene->probe_grid.aabb_min);
	shader_set_vec3_loc(scene->instanced_uniforms.probe_grid_max,
	                    scene->probe_grid.aabb_max);

	if (scene->instanced_uniforms.probe_grid_dim != -1) {
		glUniform3i(scene->instanced_uniforms.probe_grid_dim,
		            scene->probe_grid.grid_dim[0],
		            scene->probe_grid.grid_dim[1],
		            scene->probe_grid.grid_dim[2]);
	}
	shader_set_int_loc(scene->instanced_uniforms.gi_mode,
	                   (int)scene->gi_mode);

	scene_bind_probe_textures(scene);

#ifdef USE_SSBO_RENDERING
	ssbo_group_draw(&scene->ssbo_group, scene->geometry.indices.size);
#else
	instanced_group_draw(&scene->instanced_group,
	                     (int)scene->geometry.indices.size);
#endif
}

static inline void stencil_begin_object_pass(void)
{
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilFunc(GL_ALWAYS, 1, DEFAULT_STENCIL_MASK);
	glStencilMask(DEFAULT_STENCIL_MASK);
}

void scene_render(Scene* scene, GPUProfiler* profiler, mat4 view, mat4 proj,
                  vec3 camera_pos, mat4 previous_view_proj, int width,
                  int height)
{
	mat4 view_proj;
	mat4 inv_view_proj;
	glm_mat4_mul(proj, view, view_proj);
	glm_mat4_inv(view_proj, inv_view_proj);

	/* GI Probe SSBO sync — must happen before Spheres read it */
	if (scene->gi_mode != GI_MODE_OFF || scene->show_probe_grid) {
		PROFILE_ZONE(gi_sync_ctx, "GI Probe Sync (buffer upload)");
		light_probe_grid_sync(&scene->probe_grid);
		/* Sync clobbers 3D texture bindings on the current unit
		 * via glBindTexture(GL_TEXTURE_3D, 0) — invalidate cache
		 * so scene_bind_probe_textures() will re-bind. */
		for (int i = 0; i < SH_TEXTURE_COUNT; i++) {
			scene->bound_sh_textures[i] = 0;
		}
		PROFILE_ZONE_END(gi_sync_ctx);
	}

#ifdef USE_TRANSPARENT_BILLBOARDS
	if (scene->show_envmap) {
		GPU_STAGE_PROFILER(profiler, "Environment",
		                   GPU_PROFILER_ENV_COLOR);
		gl_debug_push_group("Skybox_Pass");
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDisable(GL_DEPTH_TEST);
		skybox_render(&scene->skybox, scene->skybox_shader,
		              scene->hdr_texture, scene->dummy_black_tex,
		              inv_view_proj, scene->env_lod);
		glEnable(GL_DEPTH_TEST);
		gl_debug_pop_group();
	}

	{
		stencil_begin_object_pass();

		if (scene->billboard_mode) {
			gl_debug_push_group("Billboard_Sort_And_Render");
			GLuint sorted_ssbo = 0;

			/* 1. Sorting Pass (CPU or GPU) */
			{
				GPU_STAGE_PROFILER(
				    profiler, "Sphere Sort",
				    GPU_PROFILER_MOTION_BLUR_COLOR);

				switch (scene->sorting_mode) {
					case SORTING_MODE_CPU_QSORT:
						sorted_ssbo =
						    billboard_sorter_sort_cpu(
						        &scene
						             ->billboard_sorter,
						        scene
						            ->billboard_instances,
						        scene
						            ->billboard_instance_count,
						        camera_pos);
						break;
					case SORTING_MODE_CPU_RADIX:
						sorted_ssbo =
						    billboard_sorter_sort_cpu_radix(
						        &scene
						             ->billboard_sorter,
						        scene
						            ->billboard_instances,
						        scene
						            ->billboard_instance_count,
						        camera_pos);
						break;
					case SORTING_MODE_GPU_BITONIC:
					default:
						sorted_ssbo =
						    billboard_sorter_sort_gpu(
						        &scene
						             ->billboard_sorter,
						        scene
						            ->billboard_instances,
						        scene
						            ->billboard_instance_count,
						        camera_pos);
						break;
				}
			}

			/* Tier 4: Sorted SSBO already bound at binding 2 by
			 * sort functions — vertex shader reads it directly
			 * via gl_InstanceID (no VBO copy needed). */
			scene->billboard_group.instance_count =
			    scene->billboard_instance_count;

			/* Legacy VBO copy only for debug wireframe overlay
			 * (debug_line_shader reads per-instance attributes) */
			if (scene->wireframe) {
				billboard_group_update_from_buffer(
				    &scene->billboard_group, sorted_ssbo,
				    scene->billboard_instance_count);
			}

			/* 2. Actual Billboard Rendering */
			{
				GPU_STAGE_PROFILER(profiler, "Billboard Render",
				                   GPU_PROFILER_SCENE_COLOR);

				glEnablei(GL_BLEND, 0);
				glBlendFunc(GL_SRC_ALPHA,
				            GL_ONE_MINUS_SRC_ALPHA);
				glDisablei(GL_BLEND, 1);

				scene_render_billboards(
				    scene, view, proj, camera_pos,
				    previous_view_proj, width, height);

				glDisablei(GL_BLEND, 0);
			}

			gl_debug_pop_group();
		} else {
			GPU_STAGE_PROFILER(profiler, "Instanced Render",
			                   GPU_PROFILER_SCENE_COLOR);
			gl_debug_push_group("Instanced_Geometry_Render");
			glPolygonMode(GL_FRONT_AND_BACK,
			              scene->wireframe ? GL_LINE : GL_FILL);

			scene_render_instanced(scene, view, proj, camera_pos,
			                       previous_view_proj);

			if (scene->wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			gl_debug_pop_group();
		}

		glDisable(GL_STENCIL_TEST);
	}
#else
	{
		stencil_begin_object_pass();

		if (scene->billboard_mode) {
			gl_debug_push_group("Billboard_Render");

			/* 1. Dummy sort (legacy/fallback path) */
			{
				GPU_STAGE_PROFILER(
				    profiler, "Sphere Sort",
				    GPU_PROFILER_MOTION_BLUR_COLOR);
				/* In fallback path, sorting is handled outside
				 * or not at all */
			}

			/* 2. Actual Billboard Rendering */
			{
				GPU_STAGE_PROFILER(profiler, "Billboard Render",
				                   GPU_PROFILER_SCENE_COLOR);
				scene_render_billboards(
				    scene, view, proj, camera_pos,
				    previous_view_proj, width, height);
			}

			gl_debug_pop_group();
		} else {
			GPU_STAGE_PROFILER(profiler, "Instanced Render",
			                   GPU_PROFILER_SCENE_COLOR);
			gl_debug_push_group("Instanced_Geometry_Render");
			glPolygonMode(GL_FRONT_AND_BACK,
			              scene->wireframe ? GL_LINE : GL_FILL);
			scene_render_instanced(scene, view, proj, camera_pos,
			                       previous_view_proj);

			if (scene->wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			}
			gl_debug_pop_group();
		}

		glDisable(GL_STENCIL_TEST);
	}

#endif

	/* --- N-Body orbital trails (rendered after spheres, into HDR FBO) ---
	 */
	if (scene->nbody_mode) {
		GPU_STAGE_PROFILER(profiler, "NBody Trails",
		                   GPU_PROFILER_NBODY_COLOR);
		gl_debug_push_group("NBody_Trails");
		trail_renderer_draw(&scene->trail_renderer, view, proj,
		                    camera_pos);
		gl_debug_pop_group();
	}

	if (scene->show_probe_grid) {
		light_probe_render_debug(&scene->probe_grid, view, proj);
	}
}

/* ---------------------------------------------------------------------------
 * N-Body simulation integration
 * ---------------------------------------------------------------------------*/

void scene_toggle_nbody(Scene* scene)
{
	scene->nbody_mode = !scene->nbody_mode;

	if (scene->nbody_mode) {
		/* Initialize simulation and trails */
		nbody_init_preset(&scene->nbody_sim);

		int count = nbody_get_count(&scene->nbody_sim);
		if (!trail_renderer_init(&scene->trail_renderer, count)) {
			scene->nbody_mode = 0;
			return;
		}

		/* Set trail colors from body albedos (HDR-scaled) */
		for (int i = 0; i < count; i++) {
			trail_renderer_set_color(
			    &scene->trail_renderer, i,
			    scene->nbody_sim.bodies[i].albedo);
		}

		/* Write initial instance data and update GPU */
		SphereInstance instances[NBODY_MAX_BODIES];
		nbody_write_instances(&scene->nbody_sim, instances);
		instanced_group_update(&scene->instanced_group, instances,
		                       count);

#ifdef USE_TRANSPARENT_BILLBOARDS
		if (scene->billboard_instances) {
			safe_memcpy(scene->billboard_instances,
			            sizeof(SphereInstance) * (size_t)count,
			            instances,
			            sizeof(SphereInstance) * (size_t)count);
			scene->billboard_instance_count = count;
		}
		scene->billboard_group.instance_count = count;
#endif
	} else {
		/* Restore original material grid — clean up before re-init
		 * to avoid leaking GPU buffers and CPU allocations */
		trail_renderer_cleanup(&scene->trail_renderer);
#ifdef USE_TRANSPARENT_BILLBOARDS
		if (scene->billboard_instances) {
			platform_aligned_free(scene->billboard_instances);
			scene->billboard_instances = NULL;
		}
		billboard_sorter_cleanup(&scene->billboard_sorter);
#endif
		instanced_group_cleanup(&scene->instanced_group);
		billboard_group_cleanup(&scene->billboard_group);
		scene_init_instancing(scene);
	}
}

void scene_nbody_update(Scene* scene, float delta_time)
{
	if (!scene->nbody_mode) {
		return;
	}

	/* Smooth time-scale transition (decelerate → pause → reverse) */
	nbody_update_time_scale(&scene->nbody_sim, delta_time);

	/* Advance physics (Velocity Verlet, O(N²) gravity) */
	{
		PROFILE_ZONE(verlet_ctx, "NBody Verlet");
		nbody_step(&scene->nbody_sim, delta_time);
		PROFILE_ZONE_END(verlet_ctx);
	}

	/* Record trail positions into ring buffers */
	{
		PROFILE_ZONE(trail_ctx, "NBody Trail Sample");
		trail_renderer_record(&scene->trail_renderer, &scene->nbody_sim,
		                      delta_time);
		PROFILE_ZONE_END(trail_ctx);
	}

	/* Build instance data and upload to GPU */
	int count = nbody_get_count(&scene->nbody_sim);
	SphereInstance instances[NBODY_MAX_BODIES];
	{
		PROFILE_ZONE(inst_ctx, "NBody Instance Build");
		nbody_write_instances(&scene->nbody_sim, instances);
		PROFILE_ZONE_END(inst_ctx);
	}
	{
		PROFILE_ZONE(upload_ctx, "NBody VBO Upload");
		instanced_group_update(&scene->instanced_group, instances,
		                       count);
		PROFILE_ZONE_END(upload_ctx);
	}

#ifdef USE_TRANSPARENT_BILLBOARDS
	if (scene->billboard_instances) {
		safe_memcpy(scene->billboard_instances,
		            sizeof(SphereInstance) * (size_t)count, instances,
		            sizeof(SphereInstance) * (size_t)count);
		scene->billboard_instance_count = count;
	}
	scene->billboard_group.instance_count = count;
#endif
}
