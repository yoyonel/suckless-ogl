#include "main.h"

#include "app.h"
#include "cli.h"
#include "gl_common.h"
#include "log.h"
#include "mem.h"
#include "tracy_manager.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
	tracy_manager_init_global();

	CliResult cli_result = cli_handle_args(argc, argv);
	if (cli_result.action == CLI_ACTION_EXIT_SUCCESS) {
		return EXIT_SUCCESS;
	}
	if (cli_result.action == CLI_ACTION_EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	App* app = NULL;
	if (posix_memalign((void**)&app, SIMD_ALIGNMENT, sizeof(App)) != 0) {
		LOG_ERROR("suckless-ogl.main",
		          "Failed to allocate memory for application");
		return EXIT_FAILURE;
	}
	*app = (App){0};

	if (!app_init(app, WINDOW_WIDTH, WINDOW_HEIGHT, "Icosphere Phong", cli_result.api)) {
		LOG_ERROR("suckless-ogl.main",
		          "Failed to initialize application");
		app_cleanup(app);
		free(app);
		return EXIT_FAILURE;
	}

	app_run(app);

	app_cleanup(app);
	free(app);

	return EXIT_SUCCESS;
}
