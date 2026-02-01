
# Skybox Rendering Technique (Equirectangular)

The engine uses direct **Equirectangular** mapping for the environment background. This is more memory-efficient as it avoids converting to and storing a generated cubemap.

### Early-Z Optimization
To maximize performance on integrated GPUs, the skybox is rendered **after** the scene objects.

1.  **Vertex Shader**: Positions the skybox triangles exactly on the far plane (`z = 1.0`).
2.  **Depth Test**: By using `glDepthFunc(GL_LEQUAL)`, the GPU automatically rejects skybox fragments that are occluded by 3D objects (like the icosphere) before launching the fragment shader.
3.  **Fragment Shader**: Performs an inverse spherical projection to sample the 2D HDR texture.

### Optimization Diagram

\dot
digraph SkyboxZ {
  rankdir=LR;
  bgcolor="transparent";
  dpi=72;

  // Suckless-Modern "Ghost" Design Tokens (Upscaled)
  node [
    shape=rect,
    style="rounded",
    fontname="Helvetica,Arial,sans-serif",
    fontsize=16,
    fillcolor="none",
    color="#414868",
    fontcolor="#c0caf5",
    penwidth=2
  ];

  edge [
    color="#565f89",
    fontname="Helvetica,Arial,sans-serif",
    fontsize=18,
    fontcolor="#9aa5ce",
    arrowsize=0.8,
    penwidth=1.2
  ];

  subgraph cluster_scene {
    label="Scene Pass";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#7dcfff";
    style="rounded";
    color="#7dcfff";
    margin=20;
    Objects [label="Draw Objects\n(Write Depth)", color="#7dcfff", fontcolor="#7dcfff"];
  }

  subgraph cluster_sky {
    label="Skybox Pass";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#bb9af7";
    style="rounded,dashed";
    color="#bb9af7";
    margin=20;
    VS [label="Vertex Shader\n(Set Z = 1.0)", color="#bb9af7", fontcolor="#bb9af7"];
    Test [label="Depth Test\n(GL_LEQUAL)", shape=diamond, color="#e0af68", fontcolor="#e0af68"];
    FS [label="Fragment Shader\n(Sample HDR)", color="#9ece6a", fontcolor="#9ece6a"];
    Discard [label="Discard\n(Occluded)", color="#f7768e", fontcolor="#f7768e"];
  }

  Objects -> Test [label="Depth Buffer"];
  VS -> Test;
  Test -> FS [label="Visible (Sky)"];
  Test -> Discard [label="Hidden (Object)"];
}
\enddot

```glsl
// Inverse Equirectangular Projection
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleEquirectangular(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv.x += 0.5;
    uv.y = 0.5 - uv.y;
    return uv;
}

void main() {
    vec3 dir = normalize(v_direction);
    vec2 uv = SampleEquirectangular(dir);
    FragColor = textureLod(environmentMap, uv, blur_lod);
}
```

### C Implementation (View Matrix)

We remove the **translation** component from the view matrix so the skybox appears infinitely far away (centered on the camera):

```c
/* Copy view and strip translation */
mat4 view_sky;
glm_mat4_copy(view, view_sky);
view_sky[3][0] = 0.0f;
view_sky[3][1] = 0.0f;
view_sky[3][2] = 0.0f;

/* Compute inverse view-projection */
mat4 inv_vp_sky;
glm_mat4_mul(proj, view_sky, inv_vp_sky);
glm_mat4_inv(inv_vp_sky, inv_vp_sky);
```

## 🔍 Technical Details

### Mipmap Sampling

Using `textureLod` with an equirectangular texture allows precise control over bluriness:
-   **LOD 0**: Sharp environment.
-   **LOD > 0**: Blurred environment (useful for PBR background or debugging).

### Orientation Correction

The inversion `uv.y = 0.5 - uv.y` is crucial to map the "top" of the HDR image to the "top" of the 3D world space.

## 🎨 Full Workflow

```c
void render_scene(App* app) {
    // 1. View Matrix without translation
    mat4 view_sky;
    glm_mat4_copy(app->view, view_sky);
    view_sky[3][0] = 0.0f;
    view_sky[3][1] = 0.0f;
    view_sky[3][2] = 0.0f;

    mat4 inv_vp_sky;
    glm_mat4_mul(app->proj, view_sky, inv_vp_sky);
    glm_mat4_inv(inv_vp_sky, inv_vp_sky);

    // 2. Render via skybox module
    skybox_render(&app->skybox, app->skybox_shader,
                  app->hdr_texture, inv_vp_sky, app->env_lod);
}
```

## 🌟 Advantages

1.  **Performance**: No complex matrix math, just zeroing 3 floats.
2.  **Simplicity**: Easy to understand and maintain.
3.  **Robustness**: Standard industry technique.
4.  **Quality**: Seamless infinite background.

## 📝 Important Notes

-   Use `glDepthFunc(GL_LEQUAL)` so the skybox is drawn at the back processing.
-   The skybox does not write significant depth.
-   The LOD (blur_lod) allows controlling the environment blur.

## 🔗 Python → C Equivalence

### Python (moderngl)
```python
view = camera.matrix
view[3][0] = 0
view[3][1] = 0
view[3][2] = 0
inv_view_proj = glm.inverse(projection * view)
```

### C (cglm)
```c
mat4 view;
glm_lookat(camera_pos, target, up, view);
view[3][0] = 0.0f;
view[3][1] = 0.0f;
view[3][2] = 0.0f;

mat4 inv_view_proj;
glm_mat4_mul(proj, view, inv_view_proj);
glm_mat4_inv(inv_view_proj, inv_view_proj);
```

**Perfectly equivalent!** ✅
