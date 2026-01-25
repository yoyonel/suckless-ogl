#ifndef WINDOW_H
#define WINDOW_H

typedef struct GLFWwindow GLFWwindow;

/**
 * Creates a GLFW window with an OpenGL context initialized.
 * Handles GLFW initialization, Window creation, and GLAD loading.
 *
 * @param width Window width
 * @param height Window height
 * @param title Window title
 * @param samples MSAA samples (0 or 1 to disable)
 * @return Pointer to the created window, or NULL on failure.
 */
GLFWwindow* window_create(int width, int height, const char* title,
                          int samples);

/**
 * Destroys the window and terminates GLFW.
 *
 * @param window The window to destroy.
 */
void window_destroy(GLFWwindow* window);

#endif /* WINDOW_H */
