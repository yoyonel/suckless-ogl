#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

#define DEFAULT_CAMERA_SPEED 15.0F
#define DEFAULT_CAMERA_SENSITIVITY 0.15F
#define DEFAULT_CAMERA_ZOOM 45.0F
#define DEFAULT_ZOOM_SPEED 1.0F
#define DEFAULT_MAX_PITCH 89.0F
#define DEFAULT_MIN_PITCH -89.0F
#define DEFAULT_MAX_ALPHA 1.0F
#define DEFAULT_ACCELERATION 10.0F
#define DEFAULT_FRICTION 0.85F
#define DEFAULT_ROTATION_SMOOTHING 0.18F
#define DEFAULT_BOBBING_FREQUENCY 2.2F
#define DEFAULT_BOBBING_AMPLITUDE 0.0004F
#define DEFAULT_MIN_VELOCITY_FOR_BOBBING 0.5F
#define DEFAULT_BOBBING_RESET_SPEED 0.95F
#define DEFAULT_MIN_VELOCITY 0.01F
#define DEFAULT_TARGET_FPS 60
#define DEFAULT_FIXED_TIMESTEP (1.0F / DEFAULT_TARGET_FPS)
#define DEFAULT_MOUSE_SMOOTHING_FACTOR 0.1F  // Valeur par défaut (0.0f à 0.9f)

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

	// Fixed timestep
	float physics_accumulator;
	float fixed_timestep;

	// Lissage souris
	float mouse_smoothing_factor;
} Camera;

void camera_init(Camera* cam, float distance, float yaw, float pitch);
void camera_update_vectors(Camera* cam);
void camera_process_keyboard(Camera* cam, float delta_time);
void camera_process_mouse(Camera* cam, float xoffset, float yoffset);
void camera_get_view_matrix(Camera* cam, mat4 view);
void camera_process_scroll(Camera* cam, float yoffset);
void camera_fixed_update(Camera* cam);

#endif
