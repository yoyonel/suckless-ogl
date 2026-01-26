#include "main.h"

#include "app.h"
#include "gl_common.h"
#include "log.h"
#include <stdlib.h>

int main(void)
{
	App* app = NULL;
	if (posix_memalign((void**)&app, SIMD_ALIGNMENT, sizeof(App)) != 0) {
		LOG_ERROR("suckless-ogl.main",
		          "Failed to allocate memory for application");
		return EXIT_FAILURE;
	}

	if (!app_init(app, WINDOW_WIDTH, WINDOW_HEIGHT, "Icosphere Phong")) {
		LOG_ERROR("suckless-ogl.main",
		          "Failed to initialize application");
		free(app);
		return EXIT_FAILURE;
	}

	app_run(app);
	app_cleanup(app);
	free(app);

	return EXIT_SUCCESS;
}
