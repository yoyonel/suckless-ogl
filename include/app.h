#ifndef APP_H
#define APP_H

#include "fps.h"
#include "gl_common.h"
#include "icosphere.h"
#ifdef USE_SSBO_RENDERING
#include "ssbo_rendering.h"
#endif
#include "camera.h"
#include "instanced_rendering.h"
#include "material.h"
#include "postprocess.h"
#include "shader.h"
#include "skybox.h"
#include "texture.h"
#include "ui.h"
#include <cglm/cglm.h>

typedef struct {
	GLFWwindow* window;
	int width;
	int height;

	/* Window state for fullscreen toggle */
	int is_fullscreen;
	/* Debug Flags */
	int show_exposure_debug;
	int pbr_debug_mode; /* 0=None, 1=Albedo, 2=Normal... */
	int show_imgui_demo;
	int show_help; /* Debug/Help overlay */
	int show_info_overlay;
	int text_overlay_mode; /* 0=Off, 1=FPS+Position, 2=FPS+Position+Envmap,
	                          3=FPS+Position+Envmap+Exposure */
	int saved_x, saved_y;
	int saved_width, saved_height;

	/* Scene state */
	int subdivisions;
	int wireframe;

	// /* Mouse state */
	int first_mouse; /* First mouse movement flag */
	double last_mouse_x;
	double last_mouse_y;

	// /* Camera state */
	int camera_enabled; /* Mouse control enabled */
	Camera camera;

	/* Post-processing */
	PostProcess postprocess;

	/* Icosphere geometry */
	IcosphereGeometry geometry;  // CPU side
	GLuint sphere_vao;           // GPU side
	GLuint sphere_vbo;
	GLuint sphere_nbo;
	GLuint sphere_ebo;

#ifdef USE_SSBO_RENDERING
	SSBOGroup ssbo_group;
	GLuint pbr_ssbo_shader;
#endif
	/* Instanced rendering */
	InstancedGroup instanced_group;
	GLuint pbr_instanced_shader;

	/* Shaders */
	GLuint skybox_shader;

	/* Environment mapping (equirectangular) */
	GLuint hdr_texture;
	float env_lod; /* Blur level */

	/* Skybox rendering */
	Skybox skybox;

	/* FPS counter */
	FpsCounter fps_counter;
	double last_frame_time;
	double delta_time;

	/* UI */
	UIContext ui;

	GLuint spec_prefiltered_tex;  // La texture filtrée
	GLuint irradiance_tex;
	GLuint brdf_lut_tex;

	GLuint empty_vao;
	GLuint debug_shader;
	float debug_lod;
	int show_debug_tex;

	/* PBR */
	float u_metallic;
	float u_roughness;
	float u_ao;
	float u_exposure;

	MaterialLib* material_lib;

	/* Dynamic HDR Switching */
	char** hdr_files;      /* Array of filenames */
	int hdr_count;         /* Total number of HDR files */
	int current_hdr_index; /* Index of currently loaded HDR */

	/* Auto-computed threshold for IBL */
	float auto_threshold; /* Computed mean luminance for exposure */

	/* Async Exposure Readback */
	GLuint exposure_pbo;    /* Pixel Buffer Object for async read */
	float current_exposure; /* CPU-side cached exposure value */

} App;

/* Initialization and cleanup */
int app_init(App* app, int width, int height, const char* title);
void app_cleanup(App* app);

/* Main loop */
void app_run(App* app);

/* Rendering */
void app_render(App* app);
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
