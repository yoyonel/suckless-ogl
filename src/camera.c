#include "camera.h"

#include <cglm/affine.h>  // IWYU pragma: keep
#include <cglm/cam.h>
#include <cglm/types.h>
#include <cglm/util.h>
#include <cglm/vec3.h>
#include <math.h>

#define DEFAULT_CAMERA_SPEED 15.0F
#define DEFAULT_CAMERA_SENSITIVITY 0.15F
#define DEFAULT_CAMERA_ZOOM 45.0F
#define DEFAULT_ZOOM_SPEED 1.0F
#define DEFAULT_MAX_PITCH 89.0F
#define DEFAULT_MIN_PITCH -89.0F
#define DEFAULT_MAX_ALPHA 1.0F

// Paramètres physique réaliste
#define DEFAULT_ACCELERATION 10.0F  // Vitesse d'accélération
#define DEFAULT_FRICTION \
	0.85F  // Friction (0-1, plus proche de 1 = plus glissant)
#define DEFAULT_ROTATION_SMOOTHING 0.18F   // Lissage rotation (0-1)
#define DEFAULT_BOBBING_FREQUENCY 2.2F     // Fréquence balancement
#define DEFAULT_BOBBING_AMPLITUDE 0.0004F  // Amplitude balancement
#define DEFAULT_MIN_VELOCITY_FOR_BOBBING 0.5F
#define DEFAULT_BOBBING_RESET_SPEED 0.95F
#define DEFAULT_MIN_VELOCITY 0.01F

void camera_init(Camera* cam, float distance, float yaw, float pitch)
{
	glm_vec3_copy((vec3){0.0F, 0.0F, distance}, cam->position);
	glm_vec3_copy((vec3){0.0F, 1.0F, 0.0F}, cam->world_up);

	cam->yaw = yaw;
	cam->pitch = pitch;
	cam->velocity = DEFAULT_CAMERA_SPEED;
	cam->sensitivity = DEFAULT_CAMERA_SENSITIVITY;
	cam->zoom = DEFAULT_CAMERA_ZOOM;

	// Reset des inputs
	cam->move_forward = cam->move_backward = 0;
	cam->move_left = cam->move_right = 0;
	cam->move_up = cam->move_down = 0;

	// === Initialisation physique réaliste ===

	// Option 1: Inertie
	glm_vec3_zero(cam->velocity_current);
	cam->acceleration = DEFAULT_ACCELERATION;
	cam->friction = DEFAULT_FRICTION;

	// Option 2: Rotation smooth
	cam->yaw_target = yaw;
	cam->pitch_target = pitch;
	cam->rotation_smoothing = DEFAULT_ROTATION_SMOOTHING;

	// Option 3: Head bobbing
	cam->bobbing_time = 0.0F;
	cam->bobbing_frequency = DEFAULT_BOBBING_FREQUENCY;
	cam->bobbing_amplitude = DEFAULT_BOBBING_AMPLITUDE;
	cam->bobbing_enabled = 1;  // Activé par défaut

	camera_update_vectors(cam);
}

void camera_update_vectors(Camera* cam)
{
	vec3 front;

	// Utiliser les valeurs actuelles (smoothed) au lieu des targets
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
	// === OPTION 1: SYSTÈME D'INERTIE ===

	// 1. Calculer la direction cible basée sur les inputs
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

	// 2. Interpoler vers la vitesse cible (accélération progressive)
	float alpha = cam->acceleration * delta_time;
	if (alpha > DEFAULT_MAX_ALPHA) {
		alpha = DEFAULT_MAX_ALPHA;
	}

	glm_vec3_lerp(cam->velocity_current, target_velocity, alpha,
	              cam->velocity_current);

	// 3. Appliquer friction si pas d'input actif
	float target_norm = glm_vec3_norm(target_velocity);
	if (target_norm < DEFAULT_MIN_VELOCITY) {
		// Aucun input : friction décélère progressivement
		glm_vec3_scale(cam->velocity_current, cam->friction,
		               cam->velocity_current);
	}

	// 4. Déplacer la position avec la vitesse actuelle
	vec3 movement;
	glm_vec3_scale(cam->velocity_current, delta_time, movement);
	glm_vec3_add(cam->position, movement, cam->position);

	// === OPTION 3: HEAD BOBBING ===

	if (cam->bobbing_enabled) {
		// Calculer la vitesse réelle de déplacement
		float current_speed = glm_vec3_norm(cam->velocity_current);

		// Activer le bobbing seulement si on bouge significativement
		if (current_speed > DEFAULT_MIN_VELOCITY_FOR_BOBBING) {
			// Incrémenter le temps en fonction de la vitesse
			cam->bobbing_time +=
			    delta_time * current_speed / cam->velocity;

			// Calculer l'offset vertical (sinusoïdal)
			float bobbing_offset =
			    sinf(cam->bobbing_time * cam->bobbing_frequency) *
			    cam->bobbing_amplitude;

			// Appliquer directement à la position Y
			cam->position[1] += bobbing_offset;
		} else {
			// Reset progressif du temps quand on s'arrête
			cam->bobbing_time *= DEFAULT_BOBBING_RESET_SPEED;
		}
	}
}

void camera_process_mouse(Camera* cam, float xoffset, float yoffset)
{
	// === OPTION 2: ROTATION SMOOTH ===

	xoffset *= cam->sensitivity;
	yoffset *= cam->sensitivity;

	// Mettre à jour les targets au lieu des valeurs directes
	cam->yaw_target += xoffset;
	cam->pitch_target -= yoffset;  // Inversé pour un mouvement naturel

	// Clamping du pitch target
	if (cam->pitch_target > DEFAULT_MAX_PITCH) {
		cam->pitch_target = DEFAULT_MAX_PITCH;
	}
	if (cam->pitch_target < DEFAULT_MIN_PITCH) {
		cam->pitch_target = DEFAULT_MIN_PITCH;
	}

	// Interpolation exponentielle vers les targets (smooth damping)
	float alpha = cam->rotation_smoothing;
	cam->yaw = cam->yaw + ((cam->yaw_target - cam->yaw) * alpha);
	cam->pitch = cam->pitch + ((cam->pitch_target - cam->pitch) * alpha);

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