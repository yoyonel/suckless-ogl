# GPU Profiling System

The GPU profiling system in `suckless-ogl` provides high-precision timing for various stages of the rendering pipeline. It uses OpenGL Timer Queries (GL_TIMESTAMP) to measure actual GPU execution time.

## Implementation Overview

### Core Components

* **`GPUProfiler`**: The main structure managing the collection of performance samples.
* **`AdaptiveSampler`**: For each stage, maintains a history of samples to provide a smoothed "average" and filter out noise.
* **Double Buffering**: Uses two sets of query buffers to allow the CPU to read results from the *previous* frame (N-1) while recording the *current* frame (N).

### Synchronized Timing (Blocking Wait)

To ensure **no data is lost** (even when the GPU is heavily loaded or lagging), the profiler uses a **blocking wait** when retrieving results:

1. At the start of Frame N, standard double-buffering would check if Frame N-1 results are ready.
2. If not ready, standard approaches skip the data (causing gaps).
3. **Our Approach**: We explicitly wait (`glGetQueryObjectui64v`), forcing the CPU to sync with the previous frame's completion. This guarantees a continuous, gap-free timeline even during performance spikes.

### Conditional Profiling (Zero Overhead)

Since blocking waits impact performance, the system is **completely conditional**:

* **Enabled**: When the Timeline UI is visible (F3), Metrics Logging is active (F4), or **Effect Benchmark** is running.
* **Disabled**: Zero OpenGL queries are issued, and no synchronization checks are performed. The performance impact is effectively zero.

## Integration

Profiling is integrated into the main `app_render` loop.

### Usage in Code

**RAII Pattern (Recommended):**
Using the RAII macro simplifies code and ensures stages are closed correctly.

```c
/* Start of frame (Controlled by App visibility) */
gpu_profiler_begin_frame(&app->gpu_profiler, frame_index);

/* ... */

/* Record a specific stage using RAII */
{
    GPU_STAGE_PROFILER(&app->gpu_profiler, "Geometry Pass", COLOR_HEX);
    // ... Rendering commands ...
} // Stage ends automatically here
```

**Manual Pattern:**

```c
gpu_profiler_start_stage(&app->gpu_profiler, "Geometry Pass", COLOR_HEX);
    // ... Rendering commands ...
gpu_profiler_end_stage(&app->gpu_profiler);
```

## Visualization

Performance metrics are displayed in the application UI (Timeline Overlay):

* **Bars**: Visual representation of execution duration relative to the total frame time.
* **Hierarchy**: Nested stages are indented.
* **Text**: Exact duration in milliseconds (right-aligned for readability).

Controls:

* **F3**: Toggle Timeline Visibility.
* **SHIFT+F3**: Toggle Timeline Position (Top/Bottom).
* **F4**: Toggle Console Metrics Logging.

## Changelog

* **2026-04-03**:
  * **Tracy Sync Instrumentation**: Added `PROFILE_ZONE` markers around GPU query readback, GI probe SSBO sync, GPU sort dispatch chain, and PostProcess UBO upload to detect CPU-GPU pipeline stalls.
  * **GPU Utilization Study**: See [GPU Utilization Optimization](gpu_utilization_optimization.md) for the full analysis (baseline, hypotheses, Tracy measurements, revised diagnosis).

* **2026-02-08**:
  * **RAII Support**: Added `GPU_STAGE_PROFILER` macro for automatic stage management (cleaner code, safer early returns).
  * **Instrumentation**: Instrumented the **UI Overlay** stage to fill the last gap in the GPU timeline.
  * **Robustness**: Fixed overlay disappearance during resize/stalls by indefinitely holding the last valid frame state.
  * **Data Integrity**: Switched to **blocking** query retrieval for 100% data reliability even under load.
  * **Performance**: Implemented **Conditional Profiling** to achieve zero overhead when hidden.
  * **UI UX**: Right-aligned timing metrics in the timeline overlay for easier comparison.
  * **Fixes**: Resolved OpenGL texture state warnings in the UI rendering backend.
  * **Benchmark Integration**: Profiler now automatically enables itself when `effect_benchmark` is running to prevent stalls.
