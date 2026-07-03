# RenderContext Stack Allocation Analysis (July 2026)

**Date**: July 3, 2026
**Status**: Documented Design Pattern

## Context

In the heart of the engine's main loop (`app_run()` in `src/app.c`), a local `RenderContext` structure is instantiated and initialized at every single frame before calling `renderer_draw_frame()`:

```c
{
    PROFILE_ZONE(render_ctx, "App Render");
    RenderContext rctx = {
        .scene = app->scene,
        .postprocess = app->postprocess,
        .camera = &app->input->camera,
        .profiler = &app->profiling->gpu_profiler,
        .profiler_ui = &app->profiling->timeline_ui,
        .env_mgr = app->env_mgr,
        .notifier = &app->notifier,
        .effect_bench = &app->effect_bench,
        .width = app->width,
        .height = app->height,
        .delta_time = app->delta_time,
        .frame_count = app->frame_count,
        .log_gpu_metrics = app->profiling->log_gpu_metrics,
        .render_ui = app_render_ui_trampoline,
        .render_ui_data = app,
    };
    renderer_draw_frame(&rctx);
    PROFILE_ZONE_END(render_ctx);
}
```

This technical note documents the architectural motivation and verifies the performance implications (CPU, cache, and memory) of this pattern within the application's hot loop.

---

## 1. Architectural Motivation

The primary reason for using a temporary context structure is **Separation of Concerns (SoC)** and **Decoupling**:

1. **Decoupling from the `App` Orchestrator**:
   * The low-level rendering subsystem (`renderer.c` / `renderer.h`) should not depend on the high-level `App` structure. If `renderer_draw_frame` accepted an `App*` directly, `renderer.h` would have to include `app.h`, causing circular dependencies and architectural pollution.
   * `RenderContext` represents a clean parameter object containing only what the renderer needs.
2. **Unified Handling of Dynamic & Static Parameters**:
   * While some pointers remain constant (e.g. `app->scene`), several fields change **dynamically every frame**:
     * `.delta_time` and `.frame_count`
     * `.width` and `.height` (dynamic window resizing)
     * `.log_gpu_metrics` (dynamic user profiling settings)
3. **Pointers Lifetime and Safety**:
   * Building the context on-demand ensures the pointers are always fresh (Single Source of Truth), eliminating any risk of stale cached pointers if subsystems are reallocated or loaded dynamically.

---

## 2. Performance Implications (Hot Loop Audit)

Rebuilding this structure on the stack every frame is **virtually free** for the CPU and cache:

### A. Stack Allocation Cost
Local variables in C reside on the program stack. Allocation does not involve the operating system kernel or heap memory managers.
* **Instruction Cost**: The compiler reserves stack space by adjusting the stack pointer (e.g. `sub rsp, size` assembly instruction). This adjustment occurs once when entering the function/block and is a single CPU cycle operation.

### B. Size and Cache Footprint
The `RenderContext` structure consists of 10 pointers (80 bytes on 64-bit systems) and 5 scalar/callback fields.
* **Total Size**: Approximately **112 bytes** (with struct alignment padding).
* **Cache Line Usage**: This size fits within less than **two CPU L1 Cache Lines** (which are 64 bytes each).
* **L1 Cache Efficiency**: Because the `App` orchestrator and stack frame are highly active, both source fields and target addresses are extremely hot in the L1 Data Cache, making copy operations take less than a handful of CPU cycles.

### C. Compiler Optimizations (O2 / O3)
Modern optimizing compilers (like GCC or Clang) optimize this setup aggressively:
1. **Pass-by-Reference**: The structure is passed to `renderer_draw_frame(&rctx)` by pointer. In the System V AMD64 ABI, this pointer is passed via a single CPU register (`rdi`), avoiding copying the structure values during function calls.
2. **Register Allocation & Inlining**: If the compiler inlines `renderer_draw_frame()`, the structure wrapper `rctx` is optimized out completely. The compiler will map the fields directly to their registers or stack locations of the target calls, performing zero copy operations.

---

## 3. Alternative Designs Considered

* **Design A: Cache `RenderContext` inside `App`**
  * *Disadvantage*: We would save copying static pointers, but we would still have to update the dynamic variables (`.width`, `.height`, `.delta_time`, `.frame_count`) every frame. This introduces state synchronization overhead and risks pointer corruption/obsolescence if subsystems are reallocated.
* **Design B: Pass parameters as individual arguments**
  * *Disadvantage*: `renderer_draw_frame(app->scene, app->postprocess, ...)` would require 15+ arguments. This makes code unmaintainable and violates ABI guidelines (which only allow up to 6 register arguments; the rest are pushed to the stack anyway, causing more overhead than a clean struct pointer).
