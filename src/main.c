#include "app.h"
#include "log.h"

int main(void)
{
	App app;

	if (!app_init(&app, 1024, 768, "Icosphere Phong")) {
		LOG_ERROR("suckless-ogl.main", "Failed to initialize application");
		return EXIT_FAILURE;
	}

	app_run(&app);
	app_cleanup(&app);

	return EXIT_SUCCESS;
}
