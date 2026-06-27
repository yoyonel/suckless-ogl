#ifndef GL_DEBUG_H
#define GL_DEBUG_H

/**
 * @brief Configures OpenGL debug callback.
 *
 * Enables GL_DEBUG_OUTPUT and GL_DEBUG_OUTPUT_SYNCHRONOUS,
 * then registers a callback that logs messages via log.h.
 */
void setup_opengl_debug(void);

#ifndef NDEBUG
/**
 * @brief Push a named debug group onto the OpenGL debug stack.
 *
 * Wraps glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name).
 * These groups appear as hierarchical regions in RenderDoc's Event Browser.
 *
 * @param name Human-readable label for the debug region.
 */
void gl_debug_push_group(const char* name);

/**
 * @brief Pop the current debug group from the OpenGL debug stack.
 *
 * Wraps glPopDebugGroup(). Must be paired with gl_debug_push_group().
 */
void gl_debug_pop_group(void);
#else
static inline void gl_debug_push_group(const char* name)
{
	(void)name;
}

static inline void gl_debug_pop_group(void)
{
}
#endif

#endif /* GL_DEBUG_H */
