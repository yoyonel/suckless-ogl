#include <stdlib.h>

#include "app.h"
#include "log.h"

enum { WINDOW_WIDTH = 1024, WINDOW_HEIGHT = 768 };

int main(void)
{
	App app;

	if (!app_init(&app, WINDOW_WIDTH, WINDOW_HEIGHT, "Icosphere Phong")) {
		LOG_ERROR("suckless-ogl.main",
		          "Failed to initialize application");
		return EXIT_FAILURE;
	}

	app_run(&app);
	app_cleanup(&app);

	return EXIT_SUCCESS;
}
