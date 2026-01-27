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

#define SCREEN_QUAD_VERTEX_COUNT 6

/* Memory alignment for SIMD/AVX (64-byte is AVX-512 safe and L1 cache line
 * aligned) */
#define SIMD_ALIGNMENT 64

/* Debug Markers for ApiTrace / RenderDoc */
#define GL_DEBUG_PUSH(name) \
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name)
#define GL_DEBUG_POP() glPopDebugGroup()

#endif /* GL_COMMON_H */
