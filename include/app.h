#ifndef APP_H
#define APP_H

#include "fps.h"
#include "gl_common.h"
#include "icosphere.h"
#include "material.h"
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
	int saved_x, saved_y;
	int saved_width, saved_height;

	/* Scene state */
	int subdivisions;
	int wireframe;

	/* Camera state */
	float camera_yaw;   /* Horizontal rotation (radians) */
	float camera_pitch; /* Vertical rotation (radians) */
	float camera_distance;
	int camera_enabled; /* Mouse control enabled */
	int first_mouse;    /* First mouse movement flag */
	double last_mouse_x;
	double last_mouse_y;

	/* Icosphere geometry */
	IcosphereGeometry geometry;
	GLuint vao;
	GLuint vbo;
	GLuint nbo;
	GLuint ebo;

	/* Billboards */
	GLuint quad_vao;
	GLuint quad_vbo;
	GLuint quad_ebo;
	GLsizei quad_indices_size;

	/* Shaders */
	GLuint phong_shader;
	GLuint skybox_shader;

	/* Cached uniform locations (Phong) */
	GLint u_phong_mvp;
	GLint u_phong_light_dir;

	/* Environment mapping (equirectangular) */
	GLuint hdr_texture;
	float env_lod; /* Blur level */

	/* Skybox rendering */
	Skybox skybox;

	/* FPS counter */
	FpsCounter fps_counter;
	double last_frame_time;

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
	GLuint pbr_shader;
	GLuint pbr_rt_shader;
	float u_metallic;
	float u_roughness;
	float u_ao;
	float u_exposure;

	MaterialLib* material_lib;

} App;

/* Initialization and cleanup */
int app_init(App* app, int width, int height, const char* title);
void app_cleanup(App* app);

/* Main loop */
void app_run(App* app);

/* Rendering */
void app_render(App* app);
void app_render_icosphere(App* app, mat4 view_proj);
void app_render_icosphere_pbr(App* app, mat4 view, mat4 proj, vec3 camera_pos);
void app_render_pbr_instance(App* app, mat4 view, mat4 proj, vec3 camera_pos,
                             mat4 model, float metallic, float roughness,
                             vec3 albedo);
void app_render_pbr_billboard(App* app, mat4 view, mat4 invView, mat4 proj,
                              vec3 camera_pos, mat4 model, float metallic,
                              float roughness, vec3 albedo);
void app_render_ui(App* app);
void app_init_quad(App* app);
/* Input handling */
void app_handle_input(App* app);

#endif /* APP_H */
