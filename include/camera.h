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

	// === PHYSIQUE RÉALISTE ===

	// Option 1: Inertie / Momentum
	vec3 velocity_current;  // Vitesse actuelle (3D)
	float acceleration;     // Vitesse d'accélération (ex: 5.0)
	float friction;         // Coefficient de friction 0-1 (ex: 0.85)

	// Option 2: Rotation smooth
	float yaw_target;          // Orientation cible (yaw)
	float pitch_target;        // Orientation cible (pitch)
	float rotation_smoothing;  // Facteur de lissage rotation (ex: 0.15)

	// Option 3: Head bobbing
	float bobbing_time;       // Temps accumulé pour l'oscillation
	float bobbing_frequency;  // Fréquence de balancement (ex: 2.0)
	float bobbing_amplitude;  // Amplitude verticale (ex: 0.05)
	int bobbing_enabled;      // Activé/désactivé
} Camera;

void camera_init(Camera* cam, float distance, float yaw, float pitch);
void camera_update_vectors(Camera* cam);
void camera_process_keyboard(Camera* cam, float delta_time);
void camera_process_mouse(Camera* cam, float xoffset, float yoffset);
void camera_get_view_matrix(Camera* cam, mat4 view);
void camera_process_scroll(Camera* cam, float yoffset);

#endif
