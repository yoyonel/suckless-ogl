# Modern Vertex Attribute Binding

This document describes the modernization of the vertex attribute specification and binding pipeline in `suckless-ogl`, transitioning from the legacy `glVertexAttribPointer` API to the modern OpenGL 4.3+ **Vertex Attrib Binding** API (`glVertexAttribFormat`, `glVertexAttribBinding`, `glBindVertexBuffer`).

## 1. Context and Motivation

Previously, the engine relied on the classic OpenGL vertex specification model, where attribute configuration (size, type, stride, offset) was coupled with the buffer binding using `glVertexAttribPointer`. This approach had several drawbacks:
- **Integer-to-Pointer Casts**: Legacy offsets had to be passed as `const void*`, requiring macros like `utils_buffer_offset` (which internally casted integers to `void*`). This pattern triggered strict compiler and linting warnings (such as clang-tidy's `performance-no-int-to-ptr`).
- **Tight Coupling**: Any change in buffer binding required re-specifying the attribute format, leading to redundant OpenGL driver calls and state validation overhead.

### The Modern OpenGL 4.3+ Approach

Vertex Attrib Binding decouples the **format** of vertex attributes from the **source buffers**:
1. **Format Specification**: `glVertexAttribFormat` defines the structure of the attribute (size, type, relative offset inside the struct) and maps it to a logical attribute slot.
2. **Logical Binding**: `glVertexAttribBinding` links the attribute slot to a logical **binding point**.
3. **Buffer Binding**: `glBindVertexBuffer` binds a physical GPU buffer (VBO) to a logical binding point, providing the start offset and the stride.

```mermaid
graph TD
    classDef buffer fill:#1a1b26,stroke:#7aa2f7,color:#7aa2f7,stroke-width:2
    classDef slot fill:#1a1b26,stroke:#bb9af7,color:#bb9af7
    classDef bindpoint fill:#1a1b26,stroke:#e0af68,color:#e0af68

    VBO_Geom[Geometry VBO]:::buffer
    VBO_Inst[Instance VBO]:::buffer

    BP_0[Binding Point 0<br/>Stride: 3*sizeof(float)]:::bindpoint
    BP_1[Binding Point 1<br/>Stride: sizeof(SphereInstance)]:::bindpoint

    Attr_0[Attribute 0: Position]:::slot
    Attr_1[Attribute 1: Normals]:::slot
    Attr_2[Attribute 2: Albedo]:::slot
    Attr_3[Attribute 3: Metallic]:::slot

    VBO_Geom -->|glBindVertexBuffer| BP_0
    VBO_Inst -->|glBindVertexBuffer| BP_1

    Attr_0 -->|glVertexAttribBinding| BP_0
    Attr_1 -->|glVertexAttribBinding| BP_0
    Attr_2 -->|glVertexAttribBinding| BP_1
    Attr_3 -->|glVertexAttribBinding| BP_1
```

## 2. Refactoring Details

The refactoring touched the following components:

### A. Removal of `utils_buffer_offset`
The helper function `utils_buffer_offset` was completely removed from `include/utils.h` and `src/utils.c`. Offset specification now uses standard `GLuint` offset values directly in the modern APIs, eliminating `void*` casts.

### B. Standardized Binding Points
The engine adopts a clear convention for binding points within Vertex Array Objects (VAOs):
- **Binding Point 0 & 1**: Geometry vertex data (position, normal, texcoords).
- **Binding Point 2**: Instanced data (PBR Sphere parameters, Instance matrices, etc.).

### C. Refactored Modules

- **Render Utilities (`src/render_utils.c`)**:
  - `render_utils_create_fullscreen_quad` binds positions directly using `glVertexAttribFormat` and binds the quad VBO via `glBindVertexBuffer` on point 0.
  - `render_utils_setup_sphere_instance_attributes` now takes a `GLuint binding_point` and configures sphere rendering attributes (albedo, metallic, prev_center) on the specified binding point.
- **Instanced Rendering (`src/instanced_rendering.c`)**:
  - Refactored `instanced_group_bind_mesh` to bind mesh geometry (positions on point 0, normals on point 1) and instanced matrices/colors on point 2.
- **Standard & SSBO Rendering (`src/scene_render.c`, `src/ssbo_rendering.c`)**:
  - Refactored `scene_update_gpu_buffers` (icosphere VAO) and `ssbo_group_bind_mesh` (SSBO geometry VAO) to use decoupled attribute formats and buffer bindings.
- **User Interface (`src/ui.c`)**:
  - `setup_vertex_buffers` maps ImGui's vertex stream (positions, texcoords, colors) using `glVertexAttribFormat` on binding point 0.
- **Trail Renderer (`src/trail_renderer.c`)**:
  - Configures trail vertex positions and colors on binding point 0.
- **Billboard & Shockwave Rendering (`src/billboard_rendering.c`, `src/shockwave.c`)**:
  - Configures quad geometry on points 0 and 1, and instanced data on point 2.
  - Refactored `shockwave_renderer_init` to map the shockwave billboard quad layout.
- **Debug & Visualization (`src/light_probes.c`, `src/effects/fx_lut_viz.c`)**:
  - Refactored `light_probe_grid_init` (wireframe debug AABB VAO) to use VAB.
  - Refactored `fx_lut_viz_init` (3D LUT point cloud grid VAO) to use VAB.

## 3. Advantages

1. **Strict Lint Compliance**: Avoids casting integers to pointers, solving all `performance-no-int-to-ptr` warnings.
2. **Performance**: Binding buffers independently of formats reduces GPU driver state validation overhead.
3. **Clarity**: Separation of structural vertex layouts from physical VBO bindings.
