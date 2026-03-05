#include "main.h"

#include "app.h"
#include "cli.h"
#include "gl_common.h"
#include "log.h"
#include "mem.h"
#include "platform/platform_utils.h"
#include "tracy_manager.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
	tracy_manager_init_global();

	CliAction action = cli_handle_args(argc, argv);
	if (action == CLI_ACTION_EXIT_SUCCESS) {
		return EXIT_SUCCESS;
	}
	if (action == CLI_ACTION_EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	App* app = (App*)platform_aligned_alloc(sizeof(App), SIMD_ALIGNMENT);
	if (!app) {
		LOG_ERROR("suckless-ogl.main",
		          "Failed to allocate memory for application");
		return EXIT_FAILURE;
	}
	*app = (App){0};

	if (!app_init(app, WINDOW_WIDTH, WINDOW_HEIGHT, "Icosphere Phong")) {
		LOG_ERROR("suckless-ogl.main",
		          "Failed to initialize application");
		app_cleanup(app);
		platform_aligned_free(app);
		return EXIT_FAILURE;
	}

	app_run(app);

	app_cleanup(app);
	platform_aligned_free(app);

	return EXIT_SUCCESS;
}
