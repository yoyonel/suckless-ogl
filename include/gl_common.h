#ifndef GL_COMMON_H
#define GL_COMMON_H

/* IMPORTANT: GLAD must be included before any OpenGL headers */
#include "glad/glad.h"

/* Now we can include GLFW which may pull in OpenGL headers */
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdint.h>

#define BUFFER_OFFSET(offset) \
	((void*)(uintptr_t)(offset))  // NOLINT(performance-no-int-to-ptr)

#endif /* GL_COMMON_H */
