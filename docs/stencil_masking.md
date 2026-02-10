# Stencil Buffer Masking

The renderer uses the **Stencil Buffer** as a mask to differentiate between **Objects** (geometry) and the **Skybox** (background) during the post-processing phase.

## Overview

Separating background pixels from geometry is a common optimization in post-processing. Many effects, such as **Chromatic Aberration** or **Depth of Field**, may not be desirable on the skybox or can be computed more efficiently if we know which pixels belong to the 3D scene.

### Stencil Values

The buffer is cleared to `0` at the start of each frame.

| Value | Meaning | Description |
| :--- | :--- | :--- |
| `0` | **Skybox / Background** | Background pixels (infinite distance). |
| `1` | **Object / Geometry** | Main scene objects (Spheres, Billboards). |

## Technical Implementation

### Writing to the Stencil Buffer

During the main rendering pass (`app_render`), the stencil test is enabled before rendering objects:

```c
/* Mark pixels where geometry is rendered with 1 */
glEnable(GL_STENCIL_TEST);
glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
glStencilFunc(GL_ALWAYS, 1, 0xFF);
glStencilMask(0xFF);

/* Render Spheres/Billboards... */

glDisable(GL_STENCIL_TEST);
```

The skybox is rendered with the stencil test disabled (or after clearing), ensuring it remains at `0`.

### Reading in Shaders

The post-processing shader (`postprocess.frag`) accesses the stencil channel via a **Texture View** of the depth-stencil texture.

```glsl
// Access the stencil value from the texture
uint stencil = texture(stencilTexture, TexCoords).r;
bool isSkybox = (stencil == 0u);

// Use it to skip or adjust effects
if (enableChromAbbr && !isSkybox) {
    color = applyChromAbbr(TexCoords);
} else {
    color = getSceneSource(TexCoords);
}
```

## Benefits

1. **Optimization**: Expensive sampling (like Chromatic Aberration) is skipped for background pixels that often lack detail or are far away.
2. **Visual Quality**: Prevents bleeding of background colors into foreground effects or vice-versa.
3. **Flexibility**: Provides a cheap 1-bit mask that can be reused by multiple effects without extra textures.

## Debugging

You can visualize the stencil mask by enabling the `POSTFX_STENCIL_DEBUG` flag (Toggle with **F6**). This will color the screen based on the stencil value:

- **Black**: Skybox (0)
- **White**: Objects (1)
