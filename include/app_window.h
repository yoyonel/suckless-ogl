#ifndef APP_WINDOW_H
#define APP_WINDOW_H

#include "gl_common.h"
#include <stdbool.h>

/**
 * @struct AppWindow
 * @brief Groups GLFW window handle and all window/resize state.
 *
 * Extracted from App to reduce its direct field count and form a
 * cohesive sub-struct for window management (fullscreen toggle,
 * deferred resize, saved position/size for windowed restore).
 */
typedef struct {
	GLFWwindow* handle;            /**< The GLFW window context. */
	bool is_fullscreen;            /**< Fullscreen toggle state. */
	int saved_x, saved_y;          /**< Cached pos for window restore. */
	int saved_width, saved_height; /**< Cached size for window restore. */
	bool resize_pending;           /**< Deferred resize flag. */
	int pending_width;             /**< Deferred resize target width. */
	int pending_height;            /**< Deferred resize target height. */
} AppWindow;

#include "app_subsystem.h"
int app_window_subsys_init(struct App* app);
void app_window_subsys_cleanup(struct App* app);
#define APP_WINDOW_DESCRIPTOR \
	{"window", app_window_subsys_init, app_window_subsys_cleanup}

#endif /* APP_WINDOW_H */
