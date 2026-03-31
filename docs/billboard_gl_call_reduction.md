# Billboard Pass — GL Call Reduction

## Context

The `Billboard_Sort_And_Render` pass, as observed in RenderDoc, issues **~65 GL commands**
before and including the `glDrawArraysInstanced` call. Most of these are pipeline state
setup (uniforms, texture binds, blend modes, buffer copies) that can be drastically reduced.

This document tracks the tiered optimization plan and its implementation status.

## Current Breakdown (~65 calls)

| Phase | Calls | Detail |
|-------|-------|--------|
| Compute sort | 12 | `bufferSubData`, `useProgram`, 3 uniforms, 3 SSBO binds, dispatch, barrier |
| Buffer copy SSBO→VBO | 7 | bind copy source/dest, copy, 2× defensive unbind |
| Blend state | 3 | `glEnablei`, `glBlendFunc`, `glDisablei` |
| `glUseProgram` | 1 | `pbr_ibl_billboard` |
| IBL textures | 6 | 3× (`glActiveTexture` + `glBindTexture`) |
| Sampler uniforms | 3 | **Redundant** — `layout(binding=0/1/2)` already set in shader |
| Per-frame uniforms | ~12 | projection, view, prevVP, camPos, screenSize, debugMode, GI params |
| SH textures (GI) | 14 | 7× (`glActiveTexture` + `glBindTexture3D`) |
| SSBO probe | 1 | `glBindBufferBase` |
| VAO + draw | 3 | `glBindVertexArray`, `glDisable(GL_CULL_FACE)`, `glDrawArraysInstanced` |
| Cleanup | ~3 | unbind VAO, restore cull, disable blend |

## Optimization Tiers

### Tier 1 — Trivial, No Shader Changes (~9 calls saved)

**Status: In Progress**

| Optimization | Calls saved | Risk |
|-------------|-------------|------|
| Remove 3× `glUniform1i` for sampler bindings (already `layout(binding=X)` in GLSL) | 3 | None |
| Remove 2× defensive unbind after `glCopyBufferSubData` | 2 | None |

Total: **~5 calls saved** (conservative, safe subset).

### Tier 2 — UBO for Per-Frame Uniforms (~12 calls → 1)

**Status: Planned**

Replace individual `glUniform*` calls with a single **Uniform Buffer Object**, following the
existing `PostProcessUBO` pattern in `src/postprocess.c`.

```c
typedef struct {
    mat4 projection;        // offset 0
    mat4 view;              // offset 64
    mat4 previousViewProj;  // offset 128
    vec3 camPos;            // offset 192
    int  debugMode;         // offset 204
    vec2 screenSize;        // offset 208
    vec2 _pad0;             // offset 216 (std140 alignment)
    vec3 probeGridMin;      // offset 224
    int  giMode;            // offset 236
    vec3 probeGridMax;      // offset 240
    int  specularAAEnabled; // offset 252
    ivec3 probeGridDim;     // offset 256
    int   aaMode;           // offset 268
} BillboardUBO;
```

One `glBufferSubData` + `glBindBufferBase` replaces ~12 individual uniform calls.

### Tier 3 — Persistent Texture/Buffer Bindings (~21 calls saved)

**Status: Planned**

| Optimization | Calls saved |
|-------------|-------------|
| Bind IBL textures once at load (not per-frame) | 6 |
| Bind SH 3D textures once when probe grid changes (not per-frame) | 14 |
| Bind probe SSBO once when probe grid changes | 1 |

Requires tracking a "dirty" flag on probe grid updates to re-bind only when data changes.

### Tier 4 — Direct SSBO Read in Vertex Shader (~7 calls saved)

**Status: Planned**

Eliminate the `glCopyBufferSubData` SSBO→VBO copy entirely by reading sorted instances
directly via `gl_InstanceID` from the SSBO in the vertex shader. This removes the entire
copy block (bind source, bind dest, capacity check, copy, unbind).

```glsl
// In pbr_ibl_billboard.vert — replace per-instance attributes with SSBO fetch
layout(std430, binding = 2) readonly buffer SortedInstances {
    SphereInstance instances[];
};
// ...
SphereInstance inst = instances[gl_InstanceID];
```

## Projected Results

| Tier | Effort | Calls saved | Remaining |
|------|--------|-------------|-----------|
| Baseline | — | — | **~65** |
| Tier 1 | Trivial | 5 | ~60 |
| Tier 2 (UBO) | Medium | 11 | ~49 |
| Tier 3 (Persistent) | Medium | 21 | **~28** |
| Tier 4 (SSBO direct) | Medium-High | 7 | **~21** |

## Files Involved

| File | Role |
|------|------|
| `src/scene.c` | `scene_render_billboards()` — uniform setup, texture binding |
| `src/billboard_rendering.c` | `billboard_group_update_from_buffer()` — SSBO→VBO copy |
| `src/billboard_rendering.c` | `billboard_group_draw()` — VAO bind, cull state, draw call |
| `src/sphere_sorting.c` | `sphere_sorter_sort_gpu()` — compute dispatch |
| `shaders/pbr_ibl_billboard.vert` | Vertex shader — explicit `layout(binding)` for samplers |
| `shaders/pbr_ibl_billboard.frag` | Fragment shader — `layout(binding=0/1/2)` for IBL samplers |
| `shaders/sh_probe.glsl` | SH probe uniforms and texture bindings |
| `include/scene.h` | `BillboardUniforms` struct definition |
