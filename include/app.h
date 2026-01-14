#ifndef APP_H
#define APP_H

#include "gl_common.h"
#include "icosphere.h"
#include "shader.h"
#include "skybox.h"
#include "texture.h"
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
} App;

/* Initialization and cleanup */
int app_init(App* app, int width, int height, const char* title);
void app_cleanup(App* app);

/* Main loop */
void app_run(App* app);

/* Rendering */
void app_render(App* app);
void app_render_icosphere(App* app, mat4 view_proj);

/* Input handling */
void app_handle_input(App* app);

#endif /* APP_H */
