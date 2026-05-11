#ifndef APP_INPUT_STATE_H
#define APP_INPUT_STATE_H

/**
 * @file app_input_state.h
 * @brief Camera, gamepad, key-bindings, and input smoothing sub-struct.
 *
 * Extracted from app.h to reduce include fan-out.
 * Named "app_input_state" to avoid collision with the existing
 * app_input.h (AppInputContext seam, issue #204).
 */

#include "adaptive_sampler.h"
#include "app_binding.h"
#include "camera.h"
#include "gamepad_input.h"

/**
 * @struct AppInput
 * @brief Camera, gamepad, key-bindings, and input smoothing grouped together.
 */
typedef struct AppInput {
	Camera camera;        /**< View/Proj state. */
	GamepadState gamepad; /**< Controller/gamepad input state. */
	AppBindingRegistry
	    binding_registry;        /**< Key-binding overlay registry. */
	AdaptiveSampler fps_sampler; /**< Jitter compensation for input. */
	int camera_enabled;          /**< Pause camera movement. */
} AppInput;

/**
 * @brief Initialize all input sub-systems to default state.
 * @param input  Pointer to the input sub-struct.
 */
void app_input_state_init(AppInput* input);

/**
 * @brief Release all input resources.
 * @param input  Pointer to the input sub-struct.
 */
void app_input_state_cleanup(AppInput* input);

#include "app_subsystem.h"
int app_input_subsys_init(struct App* app);
void app_input_subsys_cleanup(struct App* app);
#define APP_INPUT_DESCRIPTOR \
	{"input", app_input_subsys_init, app_input_subsys_cleanup}

#endif /* APP_INPUT_STATE_H */
