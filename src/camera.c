#include "camera.h"

#include <cglm/cam.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <cglm/vec3.h>
#include <math.h>

void camera_init(Camera* cam, float distance, float yaw, float pitch)
{
	glm_vec3_copy((vec3){0.0F, 0.0F, distance}, cam->position);
	glm_vec3_copy((vec3){0.0F, 1.0F, 0.0F}, cam->world_up);

	cam->yaw = yaw;
	cam->pitch = pitch;
	cam->velocity = DEFAULT_CAMERA_SPEED;
	cam->sensitivity = DEFAULT_CAMERA_SENSITIVITY;
	cam->zoom = DEFAULT_CAMERA_ZOOM;

	cam->move_forward = cam->move_backward = 0;
	cam->move_left = cam->move_right = 0;
	cam->move_up = cam->move_down = 0;

	// Physique
	glm_vec3_zero(cam->velocity_current);
	cam->acceleration = DEFAULT_ACCELERATION;
	cam->friction = DEFAULT_FRICTION;

	// Rotation smooth
	cam->yaw_target = yaw;
	cam->pitch_target = pitch;
	cam->rotation_smoothing = DEFAULT_ROTATION_SMOOTHING;

	// Head bobbing
	cam->bobbing_time = 0.0F;
	cam->bobbing_frequency = DEFAULT_BOBBING_FREQUENCY;
	cam->bobbing_amplitude = DEFAULT_BOBBING_AMPLITUDE;
	cam->bobbing_enabled = 1;

	// Fixed timestep
	cam->physics_accumulator = 0.0F;
	cam->fixed_timestep = DEFAULT_FIXED_TIMESTEP;

	// Lissage souris
	cam->mouse_smoothing_factor = DEFAULT_MOUSE_SMOOTHING_FACTOR;

	camera_update_vectors(cam);
}

void camera_update_vectors(Camera* cam)
{
	vec3 front;
	front[0] = cosf(glm_rad(cam->yaw)) * cosf(glm_rad(cam->pitch));
	front[1] = sinf(glm_rad(cam->pitch));
	front[2] = sinf(glm_rad(cam->yaw)) * cosf(glm_rad(cam->pitch));
	glm_vec3_normalize_to(front, cam->front);

	glm_vec3_cross(cam->front, cam->world_up, cam->right);
	glm_vec3_normalize(cam->right);

	glm_vec3_cross(cam->right, cam->front, cam->up);
	glm_vec3_normalize(cam->up);
}

// NOUVELLE FONCTION : Mise à jour physique avec pas de temps fixe
void camera_fixed_update(Camera* cam)
{
	vec3 target_velocity;
	glm_vec3_zero(target_velocity);
	vec3 temp;

	if (cam->move_forward) {
		glm_vec3_scale(cam->front, cam->velocity, temp);
		glm_vec3_add(target_velocity, temp, target_velocity);
	}
	if (cam->move_backward) {
		glm_vec3_scale(cam->front, cam->velocity, temp);
		glm_vec3_sub(target_velocity, temp, target_velocity);
	}
	if (cam->move_left) {
		glm_vec3_scale(cam->right, cam->velocity, temp);
		glm_vec3_sub(target_velocity, temp, target_velocity);
	}
	if (cam->move_right) {
		glm_vec3_scale(cam->right, cam->velocity, temp);
		glm_vec3_add(target_velocity, temp, target_velocity);
	}
	if (cam->move_up) {
		glm_vec3_scale(cam->world_up, cam->velocity, temp);
		glm_vec3_add(target_velocity, temp, target_velocity);
	}
	if (cam->move_down) {
		glm_vec3_scale(cam->world_up, cam->velocity, temp);
		glm_vec3_sub(target_velocity, temp, target_velocity);
	}

	// Interpolation avec alpha basé sur fixed_timestep
	float alpha = cam->acceleration * cam->fixed_timestep;
	if (alpha > DEFAULT_MAX_ALPHA) {
		alpha = DEFAULT_MAX_ALPHA;
	}
	glm_vec3_lerp(cam->velocity_current, target_velocity, alpha,
	              cam->velocity_current);

	// Friction
	float target_norm = glm_vec3_norm(target_velocity);
	if (target_norm < DEFAULT_MIN_VELOCITY) {
		glm_vec3_scale(cam->velocity_current, cam->friction,
		               cam->velocity_current);
	}

	// Déplacement
	vec3 movement;
	glm_vec3_scale(cam->velocity_current, cam->fixed_timestep, movement);
	glm_vec3_add(cam->position, movement, cam->position);

	// Head bobbing
	if (cam->bobbing_enabled) {
		float current_speed = glm_vec3_norm(cam->velocity_current);
		if (current_speed > DEFAULT_MIN_VELOCITY_FOR_BOBBING) {
			cam->bobbing_time +=
			    cam->fixed_timestep * current_speed / cam->velocity;
			float bobbing_offset =
			    sinf(cam->bobbing_time * cam->bobbing_frequency) *
			    cam->bobbing_amplitude;
			cam->position[1] += bobbing_offset;
		} else {
			cam->bobbing_time *= DEFAULT_BOBBING_RESET_SPEED;
		}
	}
}

// Fonction à appeler dans la boucle de jeu
void camera_update(Camera* cam, float delta_time)
{
	// Accumule le temps pour la physique
	cam->physics_accumulator += delta_time;

	// Tant qu'il y a assez de temps pour une mise à jour physique
	while (cam->physics_accumulator >= cam->fixed_timestep) {
		camera_fixed_update(cam);
		cam->physics_accumulator -= cam->fixed_timestep;
	}

	// Interpolation de la rotation (pour un rendu fluide)
	float alpha = cam->rotation_smoothing;
	cam->yaw = cam->yaw + ((cam->yaw_target - cam->yaw) * alpha);
	cam->pitch = cam->pitch + ((cam->pitch_target - cam->pitch) * alpha);

	camera_update_vectors(cam);
}

// Dans camera_process_mouse :
void camera_process_mouse(Camera* cam, float xoffset, float yoffset)
{
	static float smoothed_x = 0.0F;
	static float smoothed_y = 0.0F;

	// Lissage des inputs souris (ajustable via cam->mouse_smoothing_factor)
	smoothed_x = (cam->mouse_smoothing_factor * smoothed_x) +
	             ((1.0F - cam->mouse_smoothing_factor) * xoffset);
	smoothed_y = (cam->mouse_smoothing_factor * smoothed_y) +
	             ((1.0F - cam->mouse_smoothing_factor) * yoffset);

	// Application
	cam->yaw_target += smoothed_x * cam->sensitivity;
	cam->pitch_target -= smoothed_y * cam->sensitivity;

	// Clamping
	if (cam->pitch_target > DEFAULT_MAX_PITCH) {
		cam->pitch_target = DEFAULT_MAX_PITCH;
	}
	if (cam->pitch_target < DEFAULT_MIN_PITCH) {
		cam->pitch_target = DEFAULT_MIN_PITCH;
	}
}

void camera_get_view_matrix(Camera* cam, mat4 view)
{
	vec3 target;
	glm_vec3_add(cam->position, cam->front, target);
	glm_lookat(cam->position, target, cam->up, view);
}

void camera_process_scroll(Camera* cam, float yoffset)
{
	vec3 impulse;
	glm_vec3_scale(cam->front, yoffset * DEFAULT_SCROLL_SENSITIVITY,
	               impulse);
	glm_vec3_add(cam->velocity_current, impulse, cam->velocity_current);
}
