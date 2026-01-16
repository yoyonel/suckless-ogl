#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

typedef struct {
	vec3 position;
	vec3 front;
	vec3 up;
	vec3 right;
	vec3 world_up;

	float yaw;
	float pitch;

	float velocity;
	float sensitivity;
	float zoom;

	// États de mouvement (booléens)
	int move_forward;
	int move_backward;
	int move_left;
	int move_right;
	int move_up;
	int move_down;
} Camera;

void camera_init(Camera* cam, float distance, float yaw, float pitch);
void camera_update_vectors(Camera* cam);
void camera_process_keyboard(Camera* cam, float delta_time);
void camera_process_mouse(Camera* cam, float xoffset, float yoffset);
void camera_get_view_matrix(Camera* cam, mat4 view);
void camera_process_scroll(Camera* cam, float yoffset);

#endif