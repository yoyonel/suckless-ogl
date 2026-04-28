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

### Scene Decomposition (SceneVisuals, SceneSimulation, SceneLighting)

The `Scene` struct is being decomposed into domain-aligned sub-structs:

- **`SceneVisuals`** (`include/scene.h`): Groups visual effects — `Skybox`, `TrailRenderer`, `ShockwaveRenderer`. Access via `scene->visuals.skybox`, etc.
- **`SceneSimulation`** (`include/scene.h`): Groups N-body state — `NBodySim`, `nbody_mode`. Access via `scene->simulation.nbody_sim`, etc.
- **`SceneLighting`** (`include/scene.h`): Groups IBL, probes, and materials — `IBLCoordinator`, `LightProbeGrid`, `MaterialLib*`. Access via `scene->lighting.ibl_coord`, etc.

This reduces `Scene`'s direct field count and localizes domain-specific changes.

### App Decomposition (AppProfiling, AppInput)

The `App` struct is being decomposed into domain-aligned sub-structs:

- **`AppProfiling`** (`include/app.h`): Groups profiling and metrics — `GPUProfiler`, `GPUProfilerUI`, `FpsCounter`, `TracyManager`, `GPUUsageMonitor`, `PerfModeContext`, `perf_mode_active`, `log_gpu_metrics`. Access via `app->profiling.gpu_profiler`, etc.
- **`AppInput`** (`include/app.h`): Groups camera, gamepad, key-bindings, and input smoothing — `Camera`, `GamepadState`, `AppBindingRegistry`, `AdaptiveSampler`, `camera_enabled`. Access via `app->input.camera`, etc.

This reduces `App`'s direct field count (13 fields → 2 sub-structs) and localizes domain-specific changes.

### Effect Decoupling (EffectContext)

Post-processing effects (bloom, DoF, auto-exposure, motion blur, LUT, LUT viz) are progressively decoupled from the `PostProcess` God Object via an `EffectContext` seam:

- **`EffectContext`** (`include/effects/effect_context.h`): Read-only snapshot of shared pipeline state (source texture, viewport dimensions, depth/velocity textures, exposure).
- Effects receive `(FX*, Params*, const EffectContext*)` instead of `PostProcess*`.
- This eliminates the bidirectional dependency: `postprocess.h` → `fx_*.h` (for struct embedding) remains, but `fx_*.c` → `postprocess.h` is removed.
- Currently migrated: **bloom**. Remaining effects will follow the same pattern.

## Build System

The `CMakeLists.txt` has been updated to include the new source files. The `app` executable and `test_app` integration test both link against the new modular structure.

## Performance Impact

- **Compilation**: Parallel compilation is now more effective as changes to UI don't require re-compiling the IBL logic.
- **Runtime**: Zero overhead, as functions are simply moved into separate translation units. Inlining is still possible for performance-critical functions if they were moved to headers (though not currently required).
