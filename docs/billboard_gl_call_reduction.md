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

### Tier 4 — Direct SSBO Read in Vertex Shader (~3 calls saved)

**Status: Done** ✅

Eliminated the `glCopyBufferSubData` SSBO→VBO copy by reading sorted instances
directly via `gl_InstanceID` from the SSBO in the vertex shader.

**Key insight:** the GPU sort already binds `sorted_instance_ssbo` at binding point 2
via `glBindBufferBase`. After the compute shader completes, `glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0)`
only unbinds the generic target — NOT the indexed binding point. So binding 2 is still valid.

For CPU sort paths (qsort, radix), a single `glBindBufferBase(binding 2, instance_ssbo)` is added
at the end of `upload_sorted_to_ssbo()` to match the GPU sort’s convention.

**GLSL side** — new shared include `shaders/billboard_instance_ssbo.glsl`:

```glsl
struct SphereInstance {
    mat4 model;
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float padding;
    float _pad[9];  // Match C struct 128-byte stride (SIMD_ALIGNMENT=64)
};

layout(std430, binding = 2) readonly buffer BillboardInstanceSSBO {
    SphereInstance billboard_instances[];
};
```

**Vertex shader** — `shaders/pbr_ibl_billboard.vert` now fetches per-instance data
via `gl_InstanceID` instead of per-instance vertex attributes:

```glsl
// Before (vertex attributes):
layout(location = 2) in mat4 i_model;
layout(location = 6) in vec3 i_albedo;
layout(location = 7) in vec3 i_pbr;

// After (SSBO fetch):
SphereInstance inst = billboard_instances[gl_InstanceID];
float scaleX = length(vec3(inst.model[0]));
Albedo = inst.albedo;
Metallic = inst.metallic;  // Direct access, no vec3 packing
```

**C side** — In `src/scene.c`, the `billboard_group_update_from_buffer()` call is removed
entirely (kept only for debug wireframe overlay). No additional `glBindBufferBase` needed
in the GPU sort path — binding 2 is already set by the compute dispatch.

The legacy VBO copy is kept only for the debug wireframe overlay (which uses a separate
shader with per-instance vertex attributes). In the normal rendering path, no copy occurs.

**Call savings:**

| Sort mode | Removed | Added | Net |
|-----------|---------|-------|-----|
| GPU bitonic (default) | 3 (bind read, bind write, copy) | 0 | **-3** |
| CPU qsort/radix | 3 | 1 (`glBindBufferBase` in sort) | **-2** |

## Projected Results

| Tier | Effort | Calls saved | Remaining |
|------|--------|-------------|-----------|
| Baseline | — | — | **~65** |
| Tier 1 | Trivial | 5 | ~60 |
| Tier 2 (UBO) | Medium | 11 | ~49 |
| Tier 3 (SH/SSBO) | Medium | 15 | **~34** |
| Tier 4 (SSBO direct) | Medium | 3 | **~31** |

## Performance Regression Analysis

### Tier 4 Tradeoff: Input Assembler vs SSBO Fetch

Tier 4 replaces the traditional **fixed-function Input Assembler** path (per-instance vertex
attributes fed via VBO + `glVertexAttribDivisor`) with a **manual SSBO fetch** in the vertex
shader (`billboard_instances[gl_InstanceID]`).

This is a deliberate architectural tradeoff:

| Aspect | Before (VBO + IA) | After (SSBO fetch) |
|--------|-------------------|---------------------|
| Data path | Fixed-function hardware Input Assembler | Manual `buffer load` instruction in shader |
| Bandwidth | IA may prefetch/cache attribute streams | Single coherent buffer read per invocation |
| Latency | Dedicated hardware, potentially zero-cost | ALU instruction + L2 cache hit (typically) |
| GL calls | `glBindBufferBase` + `glCopyBufferSubData` + VBO setup | 0 extra calls (binding 2 reused from sort) |

### Why This Is Safe

**1. Fragment-shader bound workload.** Each billboard sphere runs a full PBR + IBL fragment
shader with:

- 3 IBL texture lookups (irradiance cubemap, prefiltered env map, BRDF LUT)
- 7 SH probe 3D texture lookups (spherical harmonics)
- Cook-Torrance BRDF evaluation (GGX NDF, Smith geometry, Fresnel)
- Tone mapping + gamma correction

The fragment shader cost per pixel **dwarfs** any vertex-stage data fetch difference.
For a typical 1920×1080 frame with 10–100 spheres, the vertex shader runs ~4–600 times
(4–6 vertices × instances) while the fragment shader runs millions of times.

**2. Cache-friendly access pattern.** The SSBO fetch reads `billboard_instances[gl_InstanceID]`
sequentially across instances. With 128-byte aligned structs (matching cache lines), this
yields excellent L2 cache hit rates — comparable to what the Input Assembler hardware would
achieve for the same data.

**3. Negligible instance counts.** The billboard system renders 10–100 spheres. Even with a
pessimistic 10ns penalty per vertex invocation (unlikely), the total overhead would be:

$$100 \text{ instances} \times 6 \text{ vertices} \times 10\text{ns} = 6\mu\text{s}$$

This is **three orders of magnitude** below a typical 16ms frame budget.

**4. Driver overhead reduction.** The 3 GL calls removed (bind + copy + unbind) eliminate
CPU-side driver validation and command buffer recording. On draw-call-heavy scenes, this
CPU saving can exceed the theoretical GPU cost of manual fetches.

### Measuring the Impact

The project includes a `GPUProfiler` system ([src/gpu_profiler.c](gpu_profiler.c)) with
per-stage timestamp queries, but the Billboard pass currently lacks a dedicated profiling
stage. To measure the actual impact:

1. **Add `GPU_STAGE_PROFILER`** around the billboard sort+render block in `scene.c`
2. **Use the existing `EffectBenchmark` pattern** ([src/effect_benchmark.c](effect_benchmark.c)):
   warmup 30 frames, measure 120 frames, report mean ± stddev
3. **Compare branches** by running the same camera viewpoint on `master` vs `refactor/`
4. **Expected result**: delta well within noise (< 1% variation), confirming fragment-bound
   dominance

A future `--benchmark N` CLI mode could automate this comparison across branches.

### Conclusion

The SSBO fetch is a **net positive tradeoff**: negligible GPU cost (if any) in exchange for
3 fewer GL calls, elimination of the VBO copy, and a simpler data flow where the sort output
is consumed directly by the vertex shader without intermediate copies.

## Files Involved

| File | Role |
|------|------|
| `src/scene.c` | `scene_render_billboards()` — UBO upload, texture binding, SSBO bind |
| `src/billboard_rendering.c` | `billboard_group_update_from_buffer()` — legacy VBO copy (debug only) |
| `src/billboard_rendering.c` | `billboard_group_draw()` — VAO bind, cull state, draw call |
| `src/sphere_sorting.c` | `sphere_sorter_sort_gpu()` — compute dispatch |
| `shaders/billboard_instance_ssbo.glsl` | **New**: SphereInstance SSBO for direct vertex shader read (`binding = 2`) |
| `shaders/billboard_ubo.glsl` | Shared UBO block definition (`binding = 1`) |
| `shaders/pbr_ibl_billboard.vert` | Vertex shader — SSBO fetch via `gl_InstanceID` |
| `shaders/pbr_ibl_billboard.frag` | Fragment shader — includes `billboard_ubo.glsl` |
| `shaders/sh_probe.glsl` | SH probe uniforms — guarded by `#ifndef HAS_BILLBOARD_UBO` |
| `include/scene.h` | `BillboardUBO` struct + `BillboardUniforms` (SH samplers only) |
| `include/gl_common.h` | `GL_UBO_ALIGNED` / `GL_ASSERT_UBO_ALIGNMENT` |
| `include/postprocess.h` | `PostProcessUBO` — alignment guard applied |
