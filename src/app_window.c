#include "app.h"
#include "app_input.h"
#include "app_settings.h"
#include "window.h"

/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */
int app_window_subsys_init(App* app)
{
	app->win.is_fullscreen = false;
	app->win.resize_pending = false;

	app->win.handle =
	    window_create(app->width, app->height, app->title, DEFAULT_SAMPLES);
	if (!app->win.handle) {
		return 0;
	}

	glfwSwapInterval(0);
	glfwSetWindowUserPointer(app->win.handle, app);
	glfwSetKeyCallback(app->win.handle, key_callback);
	glfwSetCursorPosCallback(app->win.handle, mouse_callback);
	glfwSetScrollCallback(app->win.handle, scroll_callback);
	glfwSetFramebufferSizeCallback(app->win.handle,
	                               framebuffer_size_callback);

	return 1;
}

/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */
void app_window_subsys_cleanup(App* app)
{
	if (app->win.handle) {
		window_destroy(app->win.handle);
		app->win.handle = NULL;
	}
}
