#ifndef APP_H
#define APP_H

#include "adaptive_sampler.h"
#include "fps.h"
#include "gl_common.h"
#include "icosphere.h"
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif
#include "billboard_rendering.h"
#include "camera.h"
#include "instanced_rendering.h"
#include "material.h"
#include "perf_timer.h"
#include "postprocess.h"
#include "shader.h"
#include "skybox.h"
#include "ui.h"
#include <cglm/cglm.h>

typedef enum {
	IBL_STATE_IDLE = 0,
	IBL_STATE_LUMINANCE,
	IBL_STATE_SPECULAR_INIT,
	IBL_STATE_SPECULAR_MIPS,
	IBL_STATE_IRRADIANCE,
	IBL_STATE_DONE
} IBLState;

typedef struct {
	IBLState state;
	int current_mip;
	int total_mips;
	int width;
	int height;
	float threshold;
	GLuint pending_hdr_tex;
	GLuint pending_spec_tex;
	GLuint pending_irr_tex;
	int current_slice;
	int total_slices;
	PerfTimer global_timer;
} IBLContext;

typedef struct {
	/* 8-byte aligned fields (Pointers, doubles) */
	PostProcess postprocess;
	GLFWwindow* window;
	double last_mouse_x;
	double last_mouse_y;
	double last_frame_time;
	double delta_time;
	uint64_t frame_count;
	Shader* pbr_instanced_shader;
	Shader* pbr_billboard_shader;
	Shader* debug_shader;
	MaterialLib* material_lib;
	char** hdr_files;

	/* Larger structs (internal alignment) */
	FpsCounter fps_counter;
	IcosphereGeometry geometry;
	AdaptiveSampler fps_sampler;
	UIContext ui;
	InstancedGroup instanced_group;
	BillboardGroup billboard_group;
	Skybox skybox;
	Camera camera;
	IBLContext ibl_ctx;

	/* 4-byte fields (int, float, GLuint) */
	int width;
	int height;
	int is_fullscreen;
	int show_exposure_debug;
	int pbr_debug_mode;
	int show_imgui_demo;
	int show_help;
	int show_info_overlay;
	int text_overlay_mode;
	int saved_x, saved_y;
	int saved_width, saved_height;
	int subdivisions;
	int wireframe;
	int show_envmap;
	int first_mouse;
	int camera_enabled;
	int billboard_mode;
	int show_debug_tex;
	int hdr_count;
	int current_hdr_index;
	int env_map_loading;

	GLuint sphere_vao;
	GLuint sphere_vbo;
	GLuint sphere_nbo;
	GLuint sphere_ebo;
	GLuint quad_vbo;
	GLuint skybox_shader;
	GLuint hdr_texture;
	GLuint spec_prefiltered_tex;
	GLuint irradiance_tex;
	GLuint brdf_lut_tex;
	GLuint empty_vao;
	GLuint shader_spmap;
	GLuint shader_irmap;
	GLuint shader_lum_pass1;
	GLuint shader_lum_pass2;
	GLuint exposure_pbo;
	GLuint dummy_black_tex;
	GLuint dummy_white_tex;
	GLuint lum_ssbo[2];

	float env_lod;
	float debug_lod;
	float u_metallic;
	float u_roughness;
	float u_ao;
	float u_exposure;
	float auto_threshold;
	float current_exposure;

#ifdef USE_SSBO_RENDERING
	SSBOGroup ssbo_group;
	Shader* pbr_ssbo_shader;
#endif

} App;

/* Initialization and cleanup */
int app_init(App* app, int width, int height, const char* title);
void app_cleanup(App* app);

/* Main loop */
void app_run(App* app);

/* Rendering */
void app_render(App* app);
void app_update(App* app);
void app_update_gpu_buffers(App* app);
#ifdef USE_SSBO_RENDERING
void app_init_ssbo(App* app);
#endif
void app_render_ui(App* app);
void app_init_instancing(App* app);
void app_render_instanced(App* app, mat4 view, mat4 proj, vec3 camera_pos);
/* Input handling */
void app_handle_input(App* app);

#endif /* APP_H */
