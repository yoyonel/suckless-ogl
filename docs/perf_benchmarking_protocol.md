# Performance Benchmarking Protocol

To ensure that optimizations provide real, measurable gains without being skewed by system noise or driver fluctuations, a rigorous benchmarking protocol has been established.

## 1. Instrumentation Layer

The project uses a hybrid instrumentation approach to capture accurate GPU timings:

### GPU Timestamps

Instead of relying on CPU-side timers (like `glfwGetTime`), we use `glQueryCounter` with `GL_TIMESTAMP`.

* **Asynchronous Queries**: Queries are issued within the OpenGL command stream and read back several frames later to avoid stalling the pipeline.
* **Macro `GPU_STAGE_PROFILER`**: Scoped blocks define the start and end of a profiling stage.

### Adaptive Sampling

To handle temporal fluctuations, timings are fed into an `AdaptiveSampler`:

* **Window**: 0.5 seconds of execution.
* **Evolving Average**: The sampler calculates the mean duration over the window, filtering out isolated "spikes" while remaining responsive to sustained performance changes.

---

## 2. Statistical Methodology

One-off measurements are often misleading due to modern GPU power management (DVFS) and background OS tasks. Our protocol enforces:

### Multiple Iterations

Every benchmark consists of **10 distinct application runs**.

### Warmup Period

Modern drivers perform JIT (Just-In-Time) compilation and optimization during the first few seconds of high load.

* **128 Warmup Frames**: The benchmark script forces 128 frames of execution before collecting any data.
* **System Stabilization**: This ensures all shaders are resident and the hardware has boosted its clock speeds to a sustained state.

### Metrics Captured

| Metric | Purpose |
| :--- | :--- |
| **Mean (Avg)** | The primary performance indicator. |
| **StdDev (Sigma)** | Measures the "jitter". High variance indicates unstable paths or scheduling issues. |
| **Min / Max** | Detects extreme performance dips or peaks. |

---

## 3. Benchmarking Tooling

A dedicated script (`perf_benchmark.py`) automates the process:

```bash
# Example command to run a standard benchmark
python3 perf_benchmark.py "Optimized Version" "./build/tests/test_app -n test_name"
```

The script parses the application logs (tagged with `perf.gpu`) and aggregates results across all 10 runs to produce the final statistical output.

---

## 4. Analysis Categories

When reviewing results, we distinguish between three types of gains:

1. **Direct Shader Gain**: The shader code itself executes faster (measured in `ms`).
2. **Sync Tax Reduction**: Gains achieved by removing synchronization points (flushes/barriers) between stages.
3. **Ghost Gains**: Improvements that appear in "Total Frame Time" but not in a specific stage, usually caused by reduced driver overhead or better hardware utilization by reducing state changes (like FBO swaps).
