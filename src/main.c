#include <stdio.h>
#include <stdlib.h>

#include "app.h"

int main(void)
{
	App app;

	if (!app_init(&app, 1024, 768, "Icosphere Phong")) {
		fprintf(stderr, "Failed to initialize application\n");
		return EXIT_FAILURE;
	}

	app_run(&app);
	app_cleanup(&app);

	return EXIT_SUCCESS;
}
