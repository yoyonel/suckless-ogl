#include "window.h"

#include "glad/glad.h"
#include "log.h"
#include <GLFW/glfw3.h>
#include <stdio.h>

GLFWwindow* window_create(int width, int height, const char* title, int samples)
{
	/* Initialize GLFW */
	if (!glfwInit()) {
		LOG_ERROR("suckless-ogl.window", "Failed to initialize GLFW");
		return NULL;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	if (samples > 1) {
		glfwWindowHint(GLFW_SAMPLES, samples);
	}

	GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (!window) {
		LOG_ERROR("suckless-ogl.window", "Failed to create window");
		glfwTerminate();
		return NULL;
	}

	glfwMakeContextCurrent(window);

	/* Initialize GLAD */
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		LOG_ERROR("suckless-ogl.window", "Failed to initialize GLAD");
		glfwDestroyWindow(window);
		glfwTerminate();
		return NULL;
	}

	/* Log Context Info */
	int major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
	int minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
	LOG_INFO("suckless-ogl.window", "Context Version: %d.%d", major, minor);
	LOG_INFO("suckless-ogl.window", "Renderer: %s",
	         glGetString(GL_RENDERER));
	LOG_INFO("suckless-ogl.window", "Version: %s", glGetString(GL_VERSION));

	return window;
}

void window_destroy(GLFWwindow* window)
{
	if (window) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();
}
