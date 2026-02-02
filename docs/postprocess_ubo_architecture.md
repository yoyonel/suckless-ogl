
# Post-Processing UBO Architecture

This document details the implementation of the **Uniform Buffer Object (UBO)** used to manage post-processing parameters in the engine. This transition from individual uniforms (`glUniform*`) was made to improve CPU performance and code clarity.

## 1. Overview

Instead of sending dozens of uniforms (floats, integers) one by one every frame, we bundle all configuration data (Vignette, Bloom, Exposure, FXAA, etc.) into a single contiguous memory structure.

-   **C Side**: `struct PostProcessUBO` (file `include/postprocess.h`)
-   **GPU Side**: `program Block` with `std140` layout (file `shaders/postprocess/ubo.glsl`)
-   **Transfer**: A single `glBufferSubData` call per frame.

## 2. Critical Constraints: std140 Layout

The `std140` layout imposes strict alignment rules in GPU memory. To ensure the driver reads data correctly, the C structure must mimic this alignment **byte-for-byte**.

### std140 Alignment Rules (Simplified)
1.  **Scalars (float, int, bool)**: Base alignment N (4 bytes).
2.  **Vectors (vec2)**: Alignment 2N (8 bytes).
3.  **Vectors (vec3, vec4)**: Alignment 4N (16 bytes).
4.  **Arrays / Structures**: Alignment rounded up to 16 bytes (size of a vec4).
5.  **Padding**: There is **no** implicit padding between scalars, unless necessary to respect the alignment of the next member.

### The "Array vs Scalar" Padding Trap
This is where a subtle error can occur:
-   If you declare `float padding[3]` in GLSL, `std140` treats this as an array. **Every array element must be aligned to 16 bytes**. So `padding[0]` takes 16 bytes (4 useful + 12 empty), `padding[1]` takes 16 bytes, etc.
-   **Solution**: Use individual scalar fields for GLSL padding (`float _pad1; float _pad2; ...`) so they are packed compactly (4 bytes each), matching exactly the packed `float padding[3]` in C.

### Memory Visualization (std140)

\dot
digraph UBOLayout {
  rankdir=LR;
  bgcolor="transparent";
  dpi=72;

  // Suckless-Modern "Ghost" Design Tokens (Upscaled)
  node [
    shape=record,
    fontname="Courier, monospace",
    fontsize=16,
    color="#414868",
    fontcolor="#c0caf5",
    penwidth=1.5
  ];

  edge [
    color="#7aa2f7",
    arrowsize=0.8,
    penwidth=1.5
  ];

  struct [label="{ C Structure (Memory) | { active_effects (4B) | time (4B) | _pad0[0] (4B) | _pad0[1] (4B) } | { vignette_int (4B) | vignette_std (4B) | vignette_rnd (4B) | _pad1 (4B) } }", color="#bb9af7", fontcolor="#bb9af7"];

  gpu [label="{ GPU Slot (std140) | { Slot 0 (16B) | Slot 1 (16B) } }", style="dashed", color="#7dcfff", fontcolor="#7dcfff"];

  struct -> gpu [label="Binary Match", fontname="Helvetica", fontsize=12, fontcolor="#9aa5ce"];
}
\enddot

## 3. Data Structure

### C Structure (postprocess.h)
We use explicit padding to align logical blocks to 16 bytes, facilitating memory reading and debugging.

\code{.c}
typedef struct {
    uint32_t active_effects; // 4 bytes
    float time;              // 4 bytes
    float _pad0[2];          // 8 bytes (Padding to reach 16 bytes)

    /* Vignette (16 bytes) */
    float vignette_intensity;
    float vignette_smoothness;
    float vignette_roundness;
    float _pad1;             // 4 bytes

    // ... (Grain, Exposure, etc.)

    /* FXAA (16 bytes) */
    float fxaa_quality_subpix;
    float fxaa_quality_edge_threshold;
    float fxaa_quality_edge_threshold_min;
    float _pad10;
} PostProcessUBO;
\endcode

### GLSL Block (ubo.glsl)
The block defines `std140` layout and binding 0.

> **IMPORTANT**: Padding fields must be declared individually!

\code{.glsl}
layout(std140, binding = 0) uniform PostProcessBlock {
    uint activeEffects;
    float time;
    float _pad0_0; // Matches _pad0[0] in C
    float _pad0_1; // Matches _pad0[1] in C

    /* Vignette */
    float v_intensity;
    float v_smoothness;
    float v_roundness;
    float _pad1;

    // ... (Grain, Exposure, etc.)

    /* FXAA */
    float fxaaQualitySubpix;
    float fxaaQualityEdgeThreshold;
    float fxaaQualityEdgeThresholdMin;
    float _pad10;
};
\endcode

## 4. Adding a New Parameter

To add an effect or parameter, meticulously follow these steps:

1.  **Add to C struct (`postprocess.h`)**:
    -   Add the field `float my_param`.
    -   Adjust the padding array `_padX` so the total block size remains a multiple of 16 bytes (optional but recommended for organization).

2.  **Add to GLSL block (`ubo.glsl`)**:
    -   Add the field `float my_param`.
    -   **Crucial:** Update the scalar padding fields (`_padX_0`, etc.) to match the C offsets *exactly*.

3.  **Usage in shaders**:
    -   The file `ubo.glsl` is included via `@header "postprocess/ubo.glsl"`.
    -   Access `my_param` directly (the block makes members global).
    -   For boolean flags, add a macro in `ubo.glsl`:
    \code{.glsl}
    #define enableMyEffect ((activeEffects & (1u << N)) != 0u)
    \endcode

## 5. Performance

Using the UBO allowed removing the following per-frame calls:
-   `glUseProgram` (for uploads)
-   ~30-50 calls to `glUniform1f` / `glUniform1i`
-   The driver validation associated with each call.

Now, `postprocess_end()` performs:
1.  Copy data to local struct (on stack).
2.  **A single call** to `glBufferSubData`.
