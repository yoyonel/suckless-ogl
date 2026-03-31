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

### Tier 1 — Trivial, No Shader Changes (~5 calls saved)

**Status: Done** ✅

| Optimization | Calls saved | Risk |
|-------------|-------------|------|
| Remove 3× `glUniform1i` for sampler bindings (already `layout(binding=X)` in GLSL) | 3 | None |
| Remove 2× defensive unbind after `glCopyBufferSubData` | 2 | None |

Total: **5 calls saved**. Validated in RenderDoc: 64 → 59 commands.

### Tier 2 — UBO for Per-Frame Uniforms (~12 calls → 2)

**Status: Done** ✅

Replaced ~12 individual `glUniform*` calls with a single **Uniform Buffer Object** upload
(`glBufferSubData` + `glBindBuffer`), following the existing `PostProcessUBO` pattern.

**GLSL side** — new shared include `shaders/billboard_ubo.glsl`:

```glsl
layout(std140, binding = 1) uniform BillboardBlock {
    mat4 projection;
    mat4 view;
    mat4 previousViewProj;
    vec3 camPos;      int debugMode;
    vec2 u_screenSize; vec2 _bb_pad0;
    vec3 u_ProbeGridMin; int u_GIMode;
    vec3 u_ProbeGridMax; int u_specularAAEnabled;
    ivec3 u_ProbeGridDim; int u_aaMode;
    vec3 u_GridToIdxScale; float _bb_pad1;
};
```

**C side** — `BillboardUBO` struct in `include/scene.h`, `std140`-aligned, uploaded via:

```c
BillboardUBO ubo = {0};
// ... fill fields ...
glBindBuffer(GL_UNIFORM_BUFFER, scene->billboard_ubo);
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BillboardUBO), &ubo);
```

**Conditional guard** in `shaders/sh_probe.glsl` — individual uniform declarations
wrapped in `#ifndef HAS_BILLBOARD_UBO` so the instanced pipeline (which does NOT
use the UBO) continues to work with explicit `layout(location=X)` uniforms.

#### UBO Alignment Safety

cglm's `glm_mat4_copy` uses AVX `_mm256_store_ps` which requires 32-byte alignment.
To prevent silent SIGSEGV crashes on stack-allocated UBOs:

- Generic API in `include/gl_common.h`:
  - `GL_UBO_ALIGNMENT` enum constant (32)
  - `GL_UBO_ALIGNED` attribute macro for typedef
  - `GL_ASSERT_UBO_ALIGNMENT(type)` compile-time `_Static_assert`
- Applied to both `BillboardUBO` and `PostProcessUBO`
- Any future UBO gets the same 2-line protection

### Tier 3 — Persistent SH Texture/SSBO Bindings (~15 calls saved)

**Status: In Progress**

| Optimization | Calls saved | Cachable? |
|-------------|-------------|----------|
| ~~Bind IBL textures once at load~~ | ~~6~~ | **NO** — units 0-2 are clobbered by Skybox and PostProcess passes |
| Bind SH 3D textures once when probe grid changes | 14 | **YES** — units 8-14 are exclusive to PBR passes |
| Bind probe SSBO once when probe grid changes | 1 | **YES** — binding point 3 is exclusive to PBR passes |

**Why IBL textures cannot be cached:**
In a multi-pass renderer, texture units 0-2 are shared across passes:
- `src/skybox.c` rebinds `GL_TEXTURE0` with the environment cubemap
- `src/postprocess.c` rebinds units 0-1-2 with FBO color attachments, bloom textures, etc.

Caching these bindings would require a centralized GL state tracker (over-engineering).
The SH textures (units 8-14) and probe SSBO (binding 3) are safe because no other
pass touches those units/bindings.

**Invalidation:** after `light_probe_grid_sync()` which calls `glBindTexture(GL_TEXTURE_3D, 0)`
on the current active unit, potentially clobbering a cached SH binding.

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
| Tier 3 (SH/SSBO) | Medium | 15 | **~34** |
| Tier 4 (SSBO direct) | Medium-High | 7 | **~27** |

## Files Involved

| File | Role |
|------|------|
| `src/scene.c` | `scene_render_billboards()` — UBO upload, texture binding |
| `src/billboard_rendering.c` | `billboard_group_update_from_buffer()` — SSBO→VBO copy |
| `src/billboard_rendering.c` | `billboard_group_draw()` — VAO bind, cull state, draw call |
| `src/sphere_sorting.c` | `sphere_sorter_sort_gpu()` — compute dispatch |
| `shaders/billboard_ubo.glsl` | **New** — shared UBO block definition (`binding = 1`) |
| `shaders/pbr_ibl_billboard.vert` | Vertex shader — includes `billboard_ubo.glsl` |
| `shaders/pbr_ibl_billboard.frag` | Fragment shader — includes `billboard_ubo.glsl` |
| `shaders/sh_probe.glsl` | SH probe uniforms — guarded by `#ifndef HAS_BILLBOARD_UBO` |
| `include/scene.h` | `BillboardUBO` struct + `BillboardUniforms` (SH samplers only) |
| `include/gl_common.h` | `GL_UBO_ALIGNED` / `GL_ASSERT_UBO_ALIGNMENT` generic API |
| `include/postprocess.h` | `PostProcessUBO` — alignment guard applied |
