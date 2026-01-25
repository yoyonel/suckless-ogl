#ifndef GL_DEBUG_H
#define GL_DEBUG_H

/**
 * @brief Configures OpenGL debug callback.
 *
 * Enables GL_DEBUG_OUTPUT and GL_DEBUG_OUTPUT_SYNCHRONOUS,
 * then registers a callback that logs messages via log.h.
 */
void setup_opengl_debug(void);

#endif /* GL_DEBUG_H */
