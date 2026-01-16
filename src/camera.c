#include "camera.h"

#include <cglm/affine.h>  // IWYU pragma: keep
#include <cglm/cam.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <cglm/vec3.h>
#include <math.h>

#define DEFAULT_CAMERA_SPEED 10.0F
#define DEFAULT_CAMERA_SENSITIVITY 0.1F
#define DEFAULT_CAMERA_ZOOM 45.0F
#define DEFAULT_ZOOM_SPEED 1.0F
#define DEFAULT_MAX_PITCH 89.0F
#define DEFAULT_MIN_PITCH -89.0F

void camera_init(Camera* cam, float distance, float yaw, float pitch)
{
	glm_vec3_copy((vec3){0.0F, 0.0F, distance}, cam->position);
	glm_vec3_copy((vec3){0.0F, 1.0F, 0.0F}, cam->world_up);

	cam->yaw = yaw;  // Regarde vers -Z par défaut
	cam->pitch = pitch;
	cam->velocity = DEFAULT_CAMERA_SPEED;
	cam->sensitivity = DEFAULT_CAMERA_SENSITIVITY;
	cam->zoom = DEFAULT_CAMERA_ZOOM;

	// Reset des inputs
	cam->move_forward = cam->move_backward = 0;
	cam->move_left = cam->move_right = 0;
	cam->move_up = cam->move_down = 0;

	camera_update_vectors(cam);
}

void camera_update_vectors(Camera* cam)
{
	vec3 front;
	front[0] = cosf(glm_rad(cam->yaw)) * cosf(glm_rad(cam->pitch));
	front[1] = sinf(glm_rad(cam->pitch));
	front[2] = sinf(glm_rad(cam->yaw)) * cosf(glm_rad(cam->pitch));

	glm_vec3_normalize_to(front, cam->front);

	// Right = Front x WorldUp
	glm_vec3_cross(cam->front, cam->world_up, cam->right);
	glm_vec3_normalize(cam->right);

	// Up = Right x Front
	glm_vec3_cross(cam->right, cam->front, cam->up);
	glm_vec3_normalize(cam->up);
}

void camera_process_keyboard(Camera* cam, float delta_time)
{
	float velocity = cam->velocity * delta_time;
	vec3 temp;

	if (cam->move_forward) {
		glm_vec3_scale(cam->front, velocity, temp);
		glm_vec3_add(cam->position, temp, cam->position);
	}
	if (cam->move_backward) {
		glm_vec3_scale(cam->front, velocity, temp);
		glm_vec3_sub(cam->position, temp, cam->position);
	}
	if (cam->move_left) {
		glm_vec3_scale(cam->right, velocity, temp);
		glm_vec3_sub(cam->position, temp, cam->position);
	}
	if (cam->move_right) {
		glm_vec3_scale(cam->right, velocity, temp);
		glm_vec3_add(cam->position, temp, cam->position);
	}
	if (cam->move_up) {
		glm_vec3_scale(cam->world_up, velocity, temp);
		glm_vec3_add(cam->position, temp, cam->position);
	}
	if (cam->move_down) {
		glm_vec3_scale(cam->world_up, velocity, temp);
		glm_vec3_sub(cam->position, temp, cam->position);
	}
}

void camera_process_mouse(Camera* cam, float xoffset, float yoffset)
{
	xoffset *= cam->sensitivity;
	yoffset *= cam->sensitivity;

	cam->yaw += xoffset;
	cam->pitch -= yoffset;  // Inversé pour un mouvement naturel

	if (cam->pitch > DEFAULT_MAX_PITCH) {
		cam->pitch = DEFAULT_MAX_PITCH;
	}
	if (cam->pitch < DEFAULT_MIN_PITCH) {
		cam->pitch = DEFAULT_MIN_PITCH;
	}

	camera_update_vectors(cam);
}

void camera_get_view_matrix(Camera* cam, mat4 view)
{
	vec3 target;
	glm_vec3_add(cam->position, cam->front, target);
	glm_lookat(cam->position, target, cam->up, view);
}

void camera_process_scroll(Camera* cam, float yoffset)
{
	// Sensibilité du scroll (équivalent à ton ZOOM_STEP)
	float zoom_speed = DEFAULT_ZOOM_SPEED;
	vec3 move;

	// On calcule le vecteur de déplacement : front * yoffset
	glm_vec3_scale(cam->front, yoffset * zoom_speed, move);

	// On l'ajoute à la position actuelle
	glm_vec3_add(cam->position, move, cam->position);

	// Note : Si tu préfères un zoom optique (FOV),
	// tu modifierais cam->zoom au lieu de cam->position.
}