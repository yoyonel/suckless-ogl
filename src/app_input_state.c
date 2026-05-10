#include <glad/glad.h>

#include "app_input_state.h"

#include "app.h"
#include "app_settings.h"
#include "platform/platform_utils.h"

void app_input_state_init(AppInput* input)
{
	input->camera_enabled = true;
	app_binding_registry_init(&input->binding_registry);
	camera_init(&input->camera, DEFAULT_CAMERA_DISTANCE, DEFAULT_CAMERA_YAW,
	            DEFAULT_CAMERA_PITCH);
	gamepad_input_init(&input->gamepad);
	adaptive_sampler_init(&input->fps_sampler, DEFAULT_FPS_WINDOW,
	                      DEFAULT_FPS_SAMPLER_SIZE, DEFAULT_FPS_TARGET);
}

void app_input_state_cleanup(AppInput* input)
{
	adaptive_sampler_cleanup(&input->fps_sampler);
}

int app_input_subsys_init(App* app)
{
	app->input =
	    platform_aligned_alloc(sizeof(*app->input), SIMD_ALIGNMENT);
	if (!app->input) {
		return 0;
	}
	*app->input = (AppInput){0};
	app_input_state_init(app->input);
	return 1;
}

void app_input_subsys_cleanup(App* app)
{
	if (app->input) {
		app_input_state_cleanup(app->input);
		platform_aligned_free(app->input);
		app->input = NULL;
	}
}
