# Post-Process Pipeline — Optimizations & Fixes (February 2026)

> **Date**: February 7, 2026
> **Target**: OpenGL 4.6, Mesa / Intel Iris Xe (RPL-U), unified-memory iGPU
> **Modified files**: 12 files (headers, C sources, shaders, tests)

---

## Table of Contents

1. [Summary](#summary)
2. [Bug Fixes](#1-bug-fixes)
   - [FXAA / Motion Blur — Fragment pipeline order](#11-fxaa-motion-blur-fragment-pipeline-order)
   - [Neighbor Max — Compute over-dispatch](#12-neighbor-max-compute-over-dispatch)
   - [Luminance Adaptation — Parallel reduction](#13-luminance-adaptation-parallel-reduction)
   - [Missing memory barriers](#14-missing-memory-barriers)
   - [Auto-Exposure — FBO unbind before compute](#15-auto-exposure-fbo-unbind-before-compute)
3. [Performance Optimizations](#2-performance-optimizations)
   - [UBO dirty flag — Partial upload](#21-ubo-dirty-flag-partial-upload)
   - [Sampler uniforms — Bind once](#22-sampler-uniforms-bind-once)
   - [VAO bind/unbind — Factored out](#23-vao-bindunbind-factored-out)
4. [GPU Profiler Refactoring](#3-gpu-profiler-refactoring)
   - [Double-buffering metadata](#31-double-buffering-metadata)
   - [Separating recording_count / stage_count](#32-separating-recording_count-stage_count)
   - [Restructured profiling stages](#33-restructured-profiling-stages)
5. [Modified Files](#4-modified-files)
6. [Validation](#5-validation)

---

## Summary

A series of optimizations and fixes applied to the post-processing pipeline
and the GPU profiler. Changes cover four axes:

- **Bug fixes** in effect execution order, compute shader dispatches, and synchronization barriers.
- **CPU-side optimizations** reducing redundant OpenGL calls per frame.
- **GPU profiler refactoring** to fix index conflicts between frames in the double-buffered system.
- **Profiling restructure** to separate compute cost from fragment cost in motion blur.

---

## 1. Bug Fixes

### 1.1 FXAA / Motion Blur — Fragment Pipeline Order

**File**: `shaders/postprocess.frag`

**Problem**: FXAA was applied directly on `screenTexture` *before*
Motion Blur and Chromatic Aberration. Motion Blur was therefore never
smoothed by FXAA, and FXAA did not benefit from motion-blurred content.

**Before**:
```
FXAA → (CA → MB) or direct texture
```

**After**:
```
(MB → CA) → FXAA → DoF → Bloom → ...
```

FXAA now receives color already processed by MB+CA via the `color` parameter,
instead of re-sampling `screenTexture` independently.

### 1.2 Neighbor Max — Compute Over-Dispatch

**File**: `src/effects/fx_motion_blur.c`

**Problem**: The Neighbor Max dispatch was identical to Tile Max
(`groups = ceil(pixels / 16)`), whereas Neighbor Max operates on
*tiles*, not pixels. This caused a 16× over-dispatch in X and 16× in Y (256× total).

**Before**:
```c
glDispatchCompute(groups_x, groups_y, 1);  // groups = ceil(width/16)
// For 1920×1080: 120×68 = 8160 groups (each 16×16 = 256 threads)
// Total: ~2M threads to process 8160 tiles
```

**After**:
```c
int neighbor_groups_x = (tile_count_x + 15) / 16;
int neighbor_groups_y = (tile_count_y + 15) / 16;
glDispatchCompute(neighbor_groups_x, neighbor_groups_y, 1);
// For 1920×1080: 8×5 = 40 groups → ~10K threads for 8160 tiles
```

### 1.3 Luminance Adaptation — Parallel Reduction

**File**: `shaders/lum_adapt.comp`

**Problem**: The luminance adaptation compute shader used a single
thread (`gl_GlobalInvocationID == (0,0)`) that iterated sequentially over
64×64 = 4096 texels of the luminance map.

**After**: Parallel reduction in shared memory with 256 threads (16×16),
each thread processing 16 texels (4×4 block), followed by a logarithmic
reduction `256 → 128 → 64 → ... → 1`.

```glsl
layout(local_size_x = 16, local_size_y = 16) in;
shared float sharedLogLum[256];
shared float sharedValidCount[256];

// Each thread accumulates 4×4 texels
// Parallel reduction with barrier()
for (uint s = 128u; s > 0u; s >>= 1u) { ... }
```

### 1.4 Missing Memory Barriers

**File**: `src/postprocess.c`

**Addition**: `glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT)` at the start of
`postprocess_end()`, after scene MRT rendering and before compute effects
(Bloom, DoF, AE, MB). Guarantees that writes to color, velocity, and
depth/stencil textures are visible to compute shaders.

### 1.5 Auto-Exposure — FBO Unbind Before Compute

**File**: `src/effects/fx_auto_exposure.c`

**Problem**: The downsample FBO remained bound during the adaptation compute
shader dispatch. The compute shader was reading `downsample_tex` while it was
still attached to the active FBO, creating a potential read/write conflict.

**After**: The FBO is unbound (`glBindFramebuffer(GL_FRAMEBUFFER, 0)`)
*before* the memory barrier and dispatch, ensuring rasterized data is flushed.

---

## 2. Performance Optimizations

### 2.1 UBO Dirty Flag — Partial Upload

**Files**: `include/postprocess.h`, `src/postprocess.c`

**Problem**: The `PostProcessUBO` (~300 bytes, std140 layout) was fully
rebuilt and uploaded every frame via `glBufferSubData`, even if no parameters
had changed (only `time` changes each frame).

**Solution**: Added a `bool ubo_dirty` flag to the `PostProcess` struct.

- **When `ubo_dirty = true`**: Full UBO rebuild and `sizeof(PostProcessUBO)` byte upload.
- **When `ubo_dirty = false`**: Partial 8-byte upload only
  (`active_effects` + `time`), the two first fields of the UBO.

The flag is set to `true` in the 17 parameter setters (`postprocess_set_*`)
and at initialization. `postprocess_update_time()` does **not** set the flag
since `time` is in the header that is always updated.

```c
if (post_processing->ubo_dirty) {
    PostProcessUBO ubo = { /* ... full rebuild ... */ };
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PostProcessUBO), &ubo);
    post_processing->ubo_dirty = false;
} else {
    struct { uint32_t active_effects; float time; } header = { ... };
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(header), &header);
}
```

**Gain**: In steady state (no UI interaction), the upload drops from
~300 bytes/frame to 8 bytes/frame.

### 2.2 Sampler Uniforms — Bind Once

**Files**: `src/postprocess.c`, `src/effects/fx_bloom.c`,
`src/effects/fx_dof.c`, `src/effects/fx_auto_exposure.c`,
`src/effects/fx_motion_blur.c`

**Problem**: `shader_set_int(shader, "samplerName", unit)` was called every
frame in `*_render()` functions, even though sampler uniforms are
**per-program state** (`glProgramUniform`) that never changes: each sampler
is always associated with the same texture unit.

**Solution**:

1. **Postprocess (uber-shader)**: 8 sampler→unit bindings are configured in
   `setup_sampler_uniforms()`, called once per `update_current_shader()`
   (shader change or recompilation).

2. **Effect shaders**: Sampler uniforms are configured in `*_init()` functions:
   - Bloom: `srcTexture = 0` on prefilter, downsample, upsample
   - Auto-Exposure: `sceneTexture = 0` on downsample, `lumTexture = 0` on adapt
   - Motion Blur: `velocityTexture = 0` on tile_max, `tileMaxTexture = 0` on neighbor_max
   - DoF: Reuses Bloom shaders (already configured)

**Gain**: Elimination of ~15 `glGetUniformLocation` + `glUniform1i` calls per frame.

**Note**: `silent_warnings = true` is now set *before* `update_current_shader()`
in `postprocess_compile_optimized()` to avoid warnings about missing uniforms
on compiled-out samplers.

### 2.3 VAO Bind/Unbind — Factored Out

**Files**: `src/postprocess.c`, `src/effects/fx_bloom.c`,
`src/effects/fx_dof.c`, `src/effects/fx_auto_exposure.c`

**Problem**: Each effect (Bloom, DoF, AE downsample) independently bound
and unbound the screen quad VAO (`glBindVertexArray(vao)` /
`glBindVertexArray(0)`), generating unnecessary state changes since all
fullscreen passes use the same VAO.

**Solution**: The VAO is bound **once** at the start of `postprocess_end()`
and unbound **once** after the last `glDrawArrays` (Final Composite).

```c
// Start of postprocess_end()
glBindVertexArray(post_processing->screen_quad_vao);

// ... Bloom, DoF, AE, MB, Final Composite ...

// After last draw
glBindVertexArray(0);
```

**Gain**: Elimination of ~6 `glBindVertexArray` calls per frame (3 bind +
3 unbind in Bloom, DoF, AE).

---

## 3. GPU Profiler Refactoring

### 3.1 Double-Buffering Metadata

**Files**: `include/gpu_profiler.h`, `src/gpu_profiler.c`

**Problem**: Stage metadata (name, color, depth, parent_index) was written
directly into `profiler->stages[]` during recording. This array is also read
by the UI for display. With query double-buffering, frame N write indices could
overwrite frame N-1 metadata still being read.

**Solution**: Added `GPUStageInfo stage_info[MAX_GPU_STAGES]` to each
`GPUQueryBuffer`. Metadata is written to the write buffer during
`gpu_profiler_start_stage()`, then restored to `stages[]` during readback in
`gpu_profiler_begin_frame()`.

```c
typedef struct {
    char name[MAX_GPU_STAGE_NAME];
    uint32_t color;
    int depth;
    int parent_index;
} GPUStageInfo;
```

### 3.2 Separating recording_count / stage_count

**Files**: `include/gpu_profiler.h`, `src/gpu_profiler.c`,
`tests/test_gpu_profiler.c`

**Problem**: `stage_count` served both as a recording counter (write-path)
and a display counter (read-path). After the buffer swap, it was reset to 0,
causing the UI to flicker for one frame.

**Solution**: Two distinct counters:
- `recording_count`: write counter, reset to 0 on each `begin_frame()`.
- `stage_count`: display counter, updated from the completed read buffer, **never** reset to 0.

### 3.3 Restructured Profiling Stages

**Files**: `src/postprocess.c`, `include/app_settings.h`

Post-process profiling is restructured to distinguish:

| Stage              | Content                                        |
|--------------------|------------------------------------------------|
| **Post-Process**   | Parent of all sub-stages                       |
| Bloom              | Prefilter + Downsample + Upsample              |
| DoF                | Downsample + Tent blur                         |
| Auto-Exposure      | Lum downsample + Compute adaptation            |
| MB Compute         | Tile Max + Neighbor Max (compute dispatches)   |
| **Final Composite**| Fullscreen quad: MB sampling, CA, FXAA, etc.   |

Added `GPU_PROFILER_COMPOSITE_COLOR` (Nord Frost Medium Blue, `0x81A1C1`)
to `app_settings.h`.

---

## 4. Modified Files

| File | Changes |
|---------|-------------|
| `include/postprocess.h` | Added `bool ubo_dirty` |
| `include/gpu_profiler.h` | `GPUStageInfo`, `recording_count`, docs |
| `include/perf_timer.h` | Clarified `GPUTimer` docs |
| `include/app_settings.h` | `GPU_PROFILER_COMPOSITE_COLOR` |
| `src/postprocess.c` | UBO dirty flag, `setup_sampler_uniforms()`, VAO factoring, memory barrier, profiling restructure |
| `src/gpu_profiler.c` | Metadata double-buffering, recording_count, docs |
| `src/effects/fx_bloom.c` | Sampler init, removed per-frame VAO/sampler |
| `src/effects/fx_dof.c` | Removed per-frame VAO/sampler |
| `src/effects/fx_auto_exposure.c` | Sampler init, FBO unbind, removed per-frame VAO/sampler |
| `src/effects/fx_motion_blur.c` | Sampler init, fixed dispatch, docs |
| `shaders/postprocess.frag` | FXAA/MB/CA reordering |
| `shaders/lum_adapt.comp` | Parallel reduction with 256 threads |
| `tests/test_gpu_profiler.c` | Adapted to new fields |

---

## 5. Validation

- **Build**: `make all` — 0 errors, 0 warnings
- **Tests**: `make test` — 32/32 tests passed (100%)
- **Lint**: `make lint` — All checks passed (clang-tidy)
- **Run**: `make run-release` — Visual verification OK
