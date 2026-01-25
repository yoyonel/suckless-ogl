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
#include "environment.h"
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
	int show_envmap;

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
	Shader* pbr_ssbo_shader;
#endif
	/* Instanced rendering */
	InstancedGroup instanced_group;
	Shader* pbr_instanced_shader;

	/* Billboard rendering */
	int billboard_mode;
	BillboardGroup billboard_group;  // Dedicated group
	GLuint quad_vbo;
	Shader* pbr_billboard_shader;

	/* Shaders */
	GLuint skybox_shader; /* Remains GLuint for now (Skybox module) */

	/* Environment module */
	Environment env;
	float env_lod; /* Blur level */

	/* Skybox rendering */
	Skybox skybox;

	/* FPS counter */
	FpsCounter fps_counter;
	AdaptiveSampler fps_sampler; /* Adaptive Frame Time Sampler */
	double last_frame_time;
	double delta_time;

	/* UI */
	UIContext ui;

	GLuint empty_vao;
	Shader* debug_shader;
	float debug_lod;
	int show_debug_tex;

	MaterialLib* material_lib;
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
