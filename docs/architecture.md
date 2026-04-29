# Architecture Documentation - Refactoring Core

This document describes the architectural changes made during the `app.c` refactoring.

## Overview

The monolithic `app.c` has been split into several specialized modules to improve maintainability, reduce compilation times, and clarify responsibilities.

### Modules

| Module | Responsibility |
| :--- | :--- |
| `app.c` / `app.h` | Orchestrator: Initialization, main loop, high-level render pass management. |
| `app_ui.c` / `app_ui.h` | UI Rendering: Overlays, help screens, debug text, histograms, and loading spinners. |
| `app_input.c` / `app_input.h` | Input Handling: Keyboard callbacks, mouse/scroll events, and post-process feature toggles. Uses `AppInputContext` (focused pointer bundle) to decouple from the `App` God Object, following the same pattern as `PostProcessInputContext`. |
| `app_env.c` / `app_env.h` | Environment & IBL: HDR file scanning, asynchronous loading, and the IBL state machine. |
| `app_scene.c` / `app_scene.h` | Scene Rendering: Billboard groups, instanced groups, and procedural geometry updates. |


## Architecture Diagram

```graphviz
digraph Architecture {
  bgcolor="transparent";
  compound=true;
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

  App [label="App\n(Core/State)", color="#e0af68", fontcolor="#e0af68", penwidth=3];

  subgraph cluster_modules {
    label="Functional Modules";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#7dcfff";
    style="rounded";
    color="#7dcfff";
    penwidth=1.5;
    margin=25;

    AppUI [label="UI (app_ui)", color="#bb9af7", fontcolor="#bb9af7"];
    AppInput [label="Input (app_input)", color="#f7768e", fontcolor="#f7768e"];
    AppEnv [label="Env/IBL (app_env)", color="#7aa2f7", fontcolor="#7aa2f7"];
    AppScene [label="Scene (app_scene)", color="#9ece6a", fontcolor="#9ece6a"];
  }

  subgraph cluster_backend {
    label="Graphics Backend";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#9aa5ce";
    style="rounded,dashed";
    color="#414868";
    margin=25;

    Shader [color="#565f89", fontcolor="#9aa5ce"];
    Texture [color="#565f89", fontcolor="#9aa5ce"];
    PBR [color="#414868", fontcolor="#c0caf5"];
    Skybox [color="#414868", fontcolor="#c0caf5"];
  }

  App -> AppUI [label="Orchestrates"];
  App -> AppInput [label="Polls"];
  App -> AppEnv [label="Updates"];
  App -> AppScene [label="Renders"];

  AppEnv -> PBR [label="Prefilters"];
  AppEnv -> Texture [label="Loads HDR"];
  AppScene -> PBR [label="Draws"];
  AppScene -> Skybox [label="Draws"];
}
```

## Data Ownership

The `App` struct (defined in `app.h`) remains the central state container. Most modules take a pointer to `App` as their first argument.

To avoid cyclic dependencies:
- Core struct definitions (`App`, `Camera`, `AsyncRequest`) use named structs instead of anonymous ones to support forward declarations.
- Module headers are included at the **end** of `app.h` to ensure they can see the full `App` definition if necessary (though they primarily use pointers).
- Specialized source files (`.c`) include `app.h` and the required renderer headers directly.

### Scene Decomposition

The `Scene` struct is fully decomposed into six domain-aligned sub-structs, each in its own header:

- **`SceneGPUResources`** (`include/scene_gpu_resources.h`): All GPU resource handles — 28 GLuint handles for textures, buffers, VAOs, compute programs, plus billboard UBO and IBL/SH binding caches. Access via `scene->gpu.hdr_texture`, `scene->gpu.icosphere_vbo`, etc.
- **`SceneShaders`** (`include/scene_shaders.h`): All shader pointers — `pbr_instanced`, `pbr_billboard`, `debug`, `debug_line`, `skybox` (+ conditional `pbr_ssbo`). Access via `scene->shaders.pbr_instanced`, etc.
- **`SceneConfig`** (`include/scene_config.h`): Runtime configuration — `wireframe`, `billboard_mode`, `sorting_mode`, `pbr_debug_mode`, `show_envmap`, `env_lod`, `subdivisions`, `gi_mode`, `show_probe_grid`, `specular_aa_enabled`, `aa_mode`. Also defines `SortingMode`, `GIMode`, `AAMode` enums. Access via `scene->config.wireframe`, etc.
- **`SceneVisuals`** (`include/scene_visuals.h`): Visual effects — `Skybox`, `TrailRenderer`, `ShockwaveRenderer`. Access via `scene->visuals.skybox`, etc.
- **`SceneSimulation`** (`include/scene_simulation.h`): N-body state — `NBodySim`, `nbody_mode`. Access via `scene->simulation.nbody_sim`, etc.
- **`SceneLighting`** (`include/scene_lighting.h`): IBL, probes, and materials — `IBLCoordinator`, `LightProbeGrid`, `MaterialLib*`. Access via `scene->lighting.ibl_coord`, etc.

This reduces `Scene`'s direct field count from ~50 to ~19 and moves domain-specific type definitions out of the monolithic `scene.h`.

### App Decomposition (AppProfiling, AppInput, AppWindow)

The `App` struct is decomposed into domain-aligned sub-structs:

- **`AppProfiling`** (`include/app_profiling.h`): Groups profiling and metrics — `GPUProfiler`, `GPUProfilerUI`, `FpsCounter`, `TracyManager`, `GPUUsageMonitor`, `PerfModeContext`, `perf_mode_active`, `log_gpu_metrics`. Access via `app->profiling->gpu_profiler`, etc. Init/cleanup delegated to `app_profiling_init()` / `app_profiling_cleanup()` in `src/app_profiling.c`.
- **`AppInput`** (`include/app_input_state.h`): Groups camera, gamepad, key-bindings, and input smoothing — `Camera`, `GamepadState`, `AppBindingRegistry`, `AdaptiveSampler`, `camera_enabled`. Access via `app->input->camera`, etc. Init/cleanup delegated to `app_input_state_init()` / `app_input_state_cleanup()` in `src/app_input_state.c`.
- **`AppWindow`** (`include/app_window.h`): Groups GLFW window handle and all window/resize state — `GLFWwindow* handle`, `is_fullscreen`, `saved_x/y`, `saved_width/height`, `resize_pending`, `pending_width/height`. Access via `app->win.handle`, `app->win.is_fullscreen`, etc.

> **Naming note**: `app_input_state.h` hosts the `AppInput` sub-struct definition, while the existing `app_input.h` hosts the `AppInputContext` seam (focused pointer bundle for input handlers, issue #204).

This reduces `App`'s direct field count from 24 to ~17. Each sub-struct header owns its type dependencies and its delegation functions, keeping `app.c` focused on orchestration.

### PostProcess Header Decomposition

The `PostProcess` struct's type definitions are decomposed into five sub-headers, following the same pattern as the Scene decomposition. `postprocess.h` (648 → 330 lines) now only contains the aggregate `PostProcess` struct, `PostProcessEffect` enum, `PostProcessPreset`, and function signatures.

- **`pp_params.h`**: All 10 uber-shader effect parameter structs (`VignetteParams`, `GrainParams`, `ExposureParams`, `ChromAbberationParams`, `WhiteBalanceParams`, `ColorGradingParams`, `TonemapParams`, `FXAAParams`, `BandingParams`, `FogParams`) plus the `BandingMode` enum and `DEFAULT_*` values. Effects with their own multi-pass pipeline (`BloomParams`, `DoFParams`, `AutoExposureParams`, `MotionBlurParams`, `LUT3DParams`) live in their respective `fx_*.h` headers.
- **`pp_ubo.h`**: GPU-side `PostProcessUBO` layout (std140) — matches the GLSL uber-shader UBO.
- **`pp_gpu_resources.h`**: `PPGPUResources` struct — FBOs, textures, UBO handle, screen quad.
- **`pp_shader_state.h`**: `PPShaderState` + `ShaderCacheEntry` — shader compilation and caching state.
- **`pp_exposure_readback.h`**: `PPExposureReadback` struct — async PBO readback for auto-exposure and histogram.

### PostProcess Translation-Unit Split

The monolithic `postprocess.c` (1634 lines) is split into six domain-focused translation units, each under 360 lines:

| TU | Lines | Responsibility |
|----|-------|----------------|
| `postprocess_init.c` | 358 | Initialization, framebuffer creation, resize, dummy textures |
| `postprocess_apply.c` | 360 | Begin/end render pass, UBO sync, bloom/DoF/AE/MB/composite passes |
| `postprocess_setters.c` | 268 | Enable/disable/toggle, parameter setters, preset application |
| `postprocess_shader.c` | 223 | Shader cache lookup, optimized compile, dynamic switching |
| `postprocess_readback.c` | 199 | PBO readback, histogram computation, luminance, matrix updates |
| `postprocess_cleanup.c` | 97 | Resource teardown (FBOs, quad, readback buffers, shader cache) |

```mermaid
graph TD
    PP_H[postprocess.h<br/><i>public API</i>]
    PP_INT[postprocess_internal.h<br/><i>shared enums + pp_ decls</i>]
    PP_INT --> PP_H
    INIT[postprocess_init.c] --> PP_INT
    APPLY[postprocess_apply.c] --> PP_INT
    SHADER[postprocess_shader.c] --> PP_INT
    CLEANUP[postprocess_cleanup.c] --> PP_INT
    SETTERS[postprocess_setters.c] --> PP_H
    READBACK[postprocess_readback.c] --> PP_H
```

**Internal header** `postprocess_internal.h` provides:

- Texture unit enums (`POSTPROCESS_TEX_UNIT_SCENE`, `_BLOOM`, `_DOF`, etc.)
- `pp_`-prefixed internal function declarations shared across TUs (e.g. `pp_create_framebuffer`, `pp_destroy_framebuffer`, `pp_setup_sampler_uniforms`, `pp_update_current_shader`, `pp_is_shader_in_cache`)
- Only included by TUs that need shared internal state; `postprocess_setters.c` and `postprocess_readback.c` only need the public `postprocess.h`

### Scene Uniform Cache Extraction

Shader uniform cache structs (`InstancedUniforms`, `DebugUniforms`, `BillboardUBO`, `BillboardUniforms`, `MAT4_FLOAT_COUNT`) are extracted from `scene.h` (194 → 120 lines) into `scene_uniforms.h`. These are GPU implementation details only accessed by `scene_render.c` and `scene_init.c`. The `Scene` struct retains its by-value uniform fields; the type definitions simply move to a focused sub-header.

### UI Layout Data Privatization

Static layout data (66-entry `KEY_LAYOUT_QWERTY[]`, 16-entry `GAMEPAD_LAYOUT[]`, ~60 visual constants, `KeyPos`/`GamepadControlPos` struct definitions) is moved from `app_ui.h` (387 → 128 lines) into `app_ui.c`. The public header now exposes only `HelpMode`, `AppUIOverlay`, and 8 function signatures. This eliminates duplicated `static const` arrays across translation units and makes layout changes recompile-local.

### GamepadContext Seam

The `GamepadContext` (`include/gamepad_context.h`) decouples `gamepad_input.c` from `camera.h`:

- Contains only the minimal camera state slice needed for gamepad input: `move_input[3]`, `yaw_target`, `pitch_target`, `fixed_timestep`, pitch limits.
- `gamepad_write_input()` takes `GamepadContext*` instead of `Camera*`.
- Bridge pattern: `Camera ↔ GamepadContext` at the call site in `app.c`.

### Effect Decoupling (EffectContext)

Post-processing effects (bloom, DoF, auto-exposure, motion blur, LUT, LUT viz) are fully decoupled from the `PostProcess` God Object via an `EffectContext` seam:

- **`EffectContext`** (`include/effects/effect_context.h`): Read-only snapshot of shared pipeline state (source texture, viewport dimensions, depth/velocity textures, exposure).
- Effects receive `(FX*, Params*, const EffectContext*)` instead of `PostProcess*`.
- This eliminates the bidirectional dependency: `postprocess.h` → `fx_*.h` (for struct embedding) remains, but `fx_*.c` → `postprocess.h` is removed.
- All effects are now fully migrated: **bloom**, **DoF**, **auto-exposure**, **motion blur**, **LUT 3D**, and **LUT viz**. No effect source file includes `postprocess.h` anymore.

### Header Dependency Cleanup (env_manager.h, renderer.h)

Two module headers carried unnecessary transitive includes, increasing coupling and recompilation cost:

- **`env_manager.h`**: Previously included `postprocess.h` even though it only uses `PostProcess*` in function signatures. Replaced with a forward declaration `typedef struct PostProcess PostProcess;`. The concrete include stays in `env_manager.c` which calls `postprocess_set_exposure_target()`.
- **`renderer.h`**: Previously included 9 project headers (`action_notifier.h`, `camera.h`, `effect_benchmark.h`, `env_manager.h`, `gpu_profiler.h`, `gpu_profiler_ui.h`, `postprocess.h`, `scene.h`, `ui.h`) even though `RenderContext` holds only pointers. All replaced with forward declarations. Only `<stdbool.h>` and `<stdint.h>` remain as concrete includes. The `.c` file includes the full headers it needs.

### Include Fan-Out Reduction (app_input.c, app_ui.c)

The two highest fan-out source files were trimmed by moving the `app_input_ctx_from_app()` bridge function to `app.c` and removing unused/transitively-redundant includes:

- **`app_input.c`**: 25 → 17 includes. The bridge function was the sole reason for including `app.h`, `app_profiling.h`, and `app_input_state.h`. Moving it to `app.c` (which already includes all three) eliminates the coupling. Additional removals: `window.h` (never used), `action_notifier.h`, `app_settings.h`, `nbody.h`, `glad/glad.h`, `GLFW/glfw3.h` (all transitively available through remaining includes).
- **`app_ui.c`**: 23 → 16 includes. Removed `stb_image.h` (zero `stbi_*` calls), `<stdio.h>` and `<stdlib.h>` (no direct usage), plus 4 transitively-redundant headers (`action_notifier.h`, `adaptive_sampler.h`, `app_binding.h`, `app_settings.h`).

**Principle**: A header that only uses a type through a pointer or reference should forward-declare that type, not include its full definition. This reduces include fan-out and speeds up incremental builds.

## Build System

The `CMakeLists.txt` has been updated to include the new source files. The `app` executable and `test_app` integration test both link against the new modular structure.

## Performance Impact

- **Compilation**: Parallel compilation is now more effective as changes to UI don't require re-compiling the IBL logic.
- **Runtime**: Zero overhead, as functions are simply moved into separate translation units. Inlining is still possible for performance-critical functions if they were moved to headers (though not currently required).

## Metrics & Health

> Last updated: April 2026 (Phase 10 — Architecture Deepening V)

### Codebase Size

| Category | Files | Total LOC |
|----------|------:|----------:|
| Sources (`src/*.c`) | 66 | 20 556 |
| Headers (`include/*.h`) | 87 | 7 839 |
| Tests (`tests/test_*.c`) | 69 | 12 666 |
| Shaders (`shaders/`) | 60 | — |

### LOC per Module (top 15)

| Module | LOC | Notes |
|--------|----:|-------|
| `postprocess.c` | 1 634 | Legacy monolith — split into 6 TUs (see PostProcess TU Split) |
| `app_ui.c` | 1 317 | UI rendering + layout data (privatized) |
| `app_input.c` | 925 | Keyboard/mouse input dispatch |
| `ui.c` | 879 | Dear ImGui integration |
| `shader.c` | 870 | Shader compilation & linking |
| `light_probes.c` | 752 | Light probe grid |
| `postprocess_presets.c` | 525 | Preset definitions |
| `scene_render.c` | 494 | Scene draw calls |
| `postprocess_input.c` | 486 | Post-process keyboard controls |
| `nbody.c` | 486 | N-body simulation (CPU + compute) |
| `scene_init.c` | 476 | Scene resource creation |
| `app.c` | 474 | Orchestrator (main loop) |
| `billboard_sorting.c` | 472 | Billboard depth sort |
| `ibl_coordinator.c` | 443 | IBL state machine |
| `async_loader.c` | 422 | Async HDR loading |

**Guideline**: modules > 500 LOC are candidates for further decomposition.

### Header Include Fan-Out

| Header | #includes | Status |
|--------|----------:|--------|
| `scene.h` | 14 | Aggregate — expected |
| `app.h` | 13 | Reduced from 22 (Phase 3) |
| `postprocess.h` | 11 | Aggregate — expected |
| `app_profiling.h` | 7 | Sub-struct — acceptable |
| `gpu_profiler.h` | 7 | Domain header |
| `utils.h` | 7 | Utility grab-bag |
| `renderer.h` | 0 | Forward-decl only ✅ |

**Principle**: aggregate headers (`app.h`, `scene.h`, `postprocess.h`) naturally have high fan-out. Leaf module headers should stay ≤ 5.

### Test Coverage (LLVM-Cov, April 2026)

| Metric | Value | Target |
|--------|------:|-------:|
| Lines | 66.6% | ≥ 78% |
| Functions | 83.1% | ≥ 85% |
| Branches | 53.6% | ≥ 50% ✅ |

68 tests pass (unit + integration, CTest).

## Include-Dependency Graph

Top-level module dependencies (project headers only, excludes system and vendor headers):

```mermaid
graph TD
    classDef aggregate fill:#1a1b26,stroke:#e0af68,color:#e0af68,stroke-width:2
    classDef substruct fill:#1a1b26,stroke:#7aa2f7,color:#7aa2f7
    classDef leaf fill:#1a1b26,stroke:#9ece6a,color:#9ece6a
    classDef effect fill:#1a1b26,stroke:#bb9af7,color:#bb9af7

    APP[app.h]:::aggregate
    SCENE[scene.h]:::aggregate
    PP[postprocess.h]:::aggregate
    REND[renderer.h]:::leaf

    %% App sub-structs
    APP_PROF[app_profiling.h]:::substruct
    APP_INP[app_input_state.h]:::substruct
    APP_WIN[app_window.h]:::substruct
    APP_UI[app_ui.h]:::substruct

    %% Scene sub-structs
    SC_GPU[scene_gpu_resources.h]:::substruct
    SC_SH[scene_shaders.h]:::substruct
    SC_CFG[scene_config.h]:::substruct
    SC_VIS[scene_visuals.h]:::substruct
    SC_SIM[scene_simulation.h]:::substruct
    SC_LIT[scene_lighting.h]:::substruct

    %% PostProcess sub-headers
    PP_PAR[pp_params.h]:::substruct
    PP_GPU[pp_gpu_resources.h]:::substruct
    PP_SHD[pp_shader_state.h]:::substruct
    PP_RDB[pp_exposure_readback.h]:::substruct

    %% Effects
    FX_BL[fx_bloom.h]:::effect
    FX_DOF[fx_dof.h]:::effect
    FX_AE[fx_auto_exposure.h]:::effect
    FX_MB[fx_motion_blur.h]:::effect
    FX_LUT[fx_lut3d.h]:::effect
    EC[effect_context.h]:::effect

    %% App → direct deps
    APP --> SCENE
    APP --> PP
    APP --> APP_UI
    APP --> APP_WIN

    %% App sub-structs (owned, not included by app.h)
    APP -.->|"owned"| APP_PROF
    APP -.->|"owned"| APP_INP

    %% Scene → sub-structs
    SCENE --> SC_GPU
    SCENE --> SC_SH
    SCENE --> SC_CFG
    SCENE --> SC_VIS
    SCENE --> SC_SIM
    SCENE --> SC_LIT

    %% PostProcess → sub-headers + effects
    PP --> PP_PAR
    PP --> PP_GPU
    PP --> PP_SHD
    PP --> PP_RDB
    PP --> FX_BL
    PP --> FX_DOF
    PP --> FX_AE
    PP --> FX_MB
    PP --> FX_LUT

    %% Effect decoupling
    FX_BL -.->|"runtime"| EC
    FX_DOF -.->|"runtime"| EC
    FX_AE -.->|"runtime"| EC
    FX_MB -.->|"runtime"| EC
    FX_LUT -.->|"runtime"| EC

    %% Renderer uses forward-decls only
    REND -.->|"fwd-decl"| APP
    REND -.->|"fwd-decl"| SCENE
    REND -.->|"fwd-decl"| PP
```

**Legend**: Solid arrows = `#include` dependency. Dashed arrows = forward declaration or runtime-only dependency. Colors: 🟡 aggregate, 🔵 sub-struct, 🟢 leaf, 🟣 effect.
