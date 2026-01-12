#ifndef APP_H
#define APP_H

#include <cglm/cglm.h>

#include "gl_common.h"
#include "icosphere.h"
#include "shader.h"
#include "skybox.h"
#include "texture.h"

typedef struct {
	GLFWwindow* window;
	int width;
	int height;

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
	GLuint compute_shader;

	/* Environment mapping */
	GLuint hdr_texture;
	GLuint env_cubemap;
	int cubemap_size;

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
