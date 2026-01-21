#ifndef GL_COMMON_H
#define GL_COMMON_H

/* IMPORTANT: GLAD must be included before any OpenGL headers */
#include "glad/glad.h"

/* Now we can include GLFW which may pull in OpenGL headers */
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdint.h>

#define BUFFER_OFFSET(offset) ((void*)((char*)0 + (offset)))

#endif /* GL_COMMON_H */
