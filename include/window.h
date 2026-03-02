#ifndef WINDOW_H
#define WINDOW_H

#include "rhi.h"

typedef struct GLFWwindow GLFWwindow;

/**
 * Creates a GLFW window. For OpenGL, it also initializes context and GLAD.
 *
 * @param width Window width
 * @param height Window height
 * @param title Window title
 * @param samples MSAA samples (0 or 1 to disable)
 * @param api The graphics API (OpenGL or Vulkan)
 * @return Pointer to the created window, or NULL on failure.
 */
GLFWwindow* window_create(int width, int height, const char* title, int samples,
                          GraphicsAPI api);

/**
 * Destroys the window and terminates GLFW.
 *
 * @param window The window to destroy.
 */
void window_destroy(GLFWwindow* window);

#endif /* WINDOW_H */
