# GPU Profiling System

The GPU profiling system in `suckless-ogl` provides high-precision timing for various stages of the rendering pipeline. It uses OpenGL Timer Queries (where available) or CPU-side markers to collect performance data without significantly impacting frame rate.

## Implementation Overview

### Core Components

* **`GPUProfiler`**: The main structure managing the collection of performance samples.
* **`AdaptiveSampler`**: Used to smooth raw timing data and provide statistical insights (min, max, average) over time.
* **OpenGL Timer Queries**: Leverages `glGenQueries`, `glBeginQuery`, and `glEndQuery` (using `GL_TIME_ELAPSED`) to measure actual GPU execution time.

### Adaptive Sampling

Raw GPU timing can be extremely noisy due to driver oscillations and thermal throttling. The system uses an `AdaptiveSampler` for each profiled event which:

1. Maintains a circular buffer of recent samples.
2. Provides a smoothed "average" for stable UI display.
3. Calculates variance to detect performance spikes.

## Integration

Profiling is integrated into the `PostProcess` and `Scene` rendering loops. Each effect (Bloom, DOF, FXAA, etc.) has its own dedicated sampler.

### Usage in Code

```c
gpu_profiler_begin_event("Bloom");
// ... Bloom rendering code ...
gpu_profiler_end_event("Bloom");
```

## Visualization

Performance metrics are displayed in the application UI:
* **Real-time Stats**: Current execution time in milliseconds.
* **ASCII Plots**: Mini-topographic plots showing performance trends over the last few seconds.
* **Summary**: Toggleable overlay showing a detailed breakdown of the frame budget.

## Performance Impact

The profiler is designed to be lightweight. Query results are retrieved asynchronously (usually with a frame delay) to avoid stalling the GPU pipeline (`GL_QUERY_RESULT_AVAILABLE`).
