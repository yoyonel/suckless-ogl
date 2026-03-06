#ifndef SCENE_RENDERER_H
#define SCENE_RENDERER_H

#include "gl_common.h"
#include <cglm/cglm.h>

/* Forward declarations */
typedef struct Scene Scene;

/**
 * @struct SceneRenderer
 * @brief Interface for different scene rendering strategies.
 */
typedef struct {
	void (*init)(Scene* scene);
	void (*render)(Scene* scene, mat4 view, mat4 proj, vec3 camera_pos,
	               mat4 previous_view_proj, int width, int height);
	void (*update_buffers)(Scene* scene);
	void (*cleanup)(Scene* scene);
} SceneRenderer;

#endif /* SCENE_RENDERER_H */
