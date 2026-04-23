# NBody Buffer Upload Benchmark

## Purpose

This benchmark provides a **repeatable, automated measurement** of the CPU-side
cost of the NBody rendering pipeline's per-frame buffer uploads. It captures
the exact code paths that were identified as sources of CPU↔GPU synchronization
stalls (see [NBody Buffer Upload Optimization](nbody_buffer_optimization.md)).

The benchmark produces a **baseline** — a reference measurement taken under
controlled conditions — that serves two roles:

1. **Validate optimizations**: confirm that a code change (e.g., buffer
   orphaning) actually reduces upload latency.
2. **Detect regressions**: if a future refactoring or feature addition
   degrades upload performance, the benchmark will catch it.

## What Is Measured

The test file `tests/test_benchmark_buffer_upload.c` contains three tests:

### 1. `test_benchmark_nbody_instance_update`

Measures the **instance update pipeline** — the code path that runs every frame
to push NBody sphere transforms and PBR materials to the GPU:

```text
nbody_step()              → O(N²) physics integration (Velocity Verlet)
nbody_write_instances()   → Build SphereInstance array from simulation state
instanced_group_update()  → Orphan + upload instance VBO (GL_DYNAMIC_DRAW)
glFinish()                → Force GPU completion for accurate timing
```

The measurement window covers `nbody_write_instances` + `instanced_group_update`
+ `glFinish` — i.e., the **build + upload + sync** cost. The physics step runs
outside the measurement to isolate upload latency from compute cost.

The benchmark uses a **draw → update** ordering: the previous frame's draw call
is submitted first, putting the VBO "in-flight" on the GPU, then the update is
measured. This reproduces the real-world scenario where orphaning matters — the
driver must handle a buffer that is still being read by the GPU.

### 2. `test_benchmark_trail_renderer`

Measures the **trail ribbon pipeline** — camera-facing ribbon geometry rebuilt
entirely on the CPU every frame:

```text
trail_renderer_record()   → Record body positions into ring buffers
trail_renderer_draw()     → Build ribbons + orphan + upload VBO + draw
glFinish()                → Force GPU completion for accurate timing
```

The measurement covers the entire `trail_renderer_draw` call because the ribbon
construction (CPU), VBO upload (CPU→GPU), and draw submission are tightly coupled
inside that function.

### 3. `test_instance_update_data_integrity`

A **correctness test** (not a benchmark). Verifies that buffer orphaning does not
corrupt data by:

1. Running a physics step through the real API
2. Uploading instances via `instanced_group_update` (which does orphan + subdata)
3. Reading back the GPU buffer with `glGetBufferSubData`
4. Comparing metallic, roughness, and albedo values within tolerance

This test exists because buffer orphaning changes the backing store — if the
driver or the application has a bug, data could be silently lost.

## Architecture: Real API, Not Synthetic GL

The benchmark uses **exclusively the real application functions**:

| Component | Functions Used |
|-----------|---------------|
| NBody simulation | `nbody_init_preset`, `nbody_step`, `nbody_get_count`, `nbody_write_instances` |
| Instance rendering | `instanced_group_init`, `instanced_group_update`, `instanced_group_draw`, `instanced_group_bind_mesh` |
| Trail rendering | `trail_renderer_init`, `trail_renderer_record`, `trail_renderer_draw`, `trail_renderer_set_color` |
| Mesh generation | `icosphere_generate`, `icosphere_free` |

The only raw GL calls are:

- **Icosphere VBO/NBO/EBO upload**: necessary because `instanced_group_bind_mesh`
  expects pre-existing buffer objects (the mesh is static geometry, not part of
  the dynamic upload path being measured).
- **`glFinish()`**: forces GPU pipeline drain to get accurate wall-clock timing.
- **`glGetBufferSubData()`**: in the integrity test, to read back GPU data.

This design means the benchmark **automatically tracks code changes**. If
`instanced_group_update` is refactored (e.g., switched to persistent mapped
buffers), the benchmark measures the new implementation with zero modifications.

## Test Parameters

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| `FRAMES` | 300 | Enough samples for statistical stability |
| `WARMUP_FRAMES` | 60 | Primes driver JIT, fills trail ring buffers (256 slots) |
| `SIMULATED_DT` | 1/60s | Matches real application frame rate |
| `BENCH_SUBDIVISIONS` | 3 | Matches `INITIAL_SUBDIVISIONS` (3840 indices) |
| Timer | `clock_gettime(CLOCK_MONOTONIC)` | Microsecond-precision, monotonic, no NTP drift |

The warmup phase is critical: without it, the first ~30 frames show inflated
timings due to driver shader compilation and buffer pool initialization.

## Baseline Results

Captured on the `perf/nbody-buffer-orphaning` branch **with orphaning applied**:

### Instance Update Pipeline

```text
=== NBody Instance Update Benchmark ===
Bodies: 14  |  Mesh: 3840 indices (subdiv 3)
Frames: 300 (warmup: 60)
Avg update+upload: 734.6 µs  (4.41% of 60fps budget)
Min: 138.6 µs  |  Max: 7450.2 µs
Total update time: 220.4 ms over 300 frames
```

### Trail Renderer Pipeline

```text
=== Trail Renderer Benchmark ===
Bodies: 14  |  Trail depth: 256 samples
Frames: 300 (warmup: 60)
Avg draw (build+upload+render): 830.2 µs  (4.98% of 60fps budget)
Min: 218.8 µs  |  Max: 5107.5 µs
Total draw time: 249.1 ms over 300 frames
```

### Interpreting the Numbers

- **Average (734/830 µs)**: the typical per-frame cost. Combined, the two
  pipelines consume ~1.56 ms or **~9.4% of the 16.67 ms frame budget** at 60 FPS.
  This leaves ~90% for all other work (PBR shading, post-processing, IBL, UI).

- **Min (138/218 µs)**: the best case — no contention, driver recycles a buffer
  immediately. This is the theoretical floor.

- **Max (7450/5107 µs)**: occasional spikes from OS scheduling, driver GC, or
  GPU clock frequency transitions. These outliers are normal in wall-clock
  benchmarks and should be evaluated against the average, not in isolation.

- **% of 60fps budget**: the key metric. If this number approaches 50%+, the
  upload pipeline is a bottleneck. Under 10% means it's healthy overhead.

### What the Baseline Represents

This CI baseline was captured on a **software-rendered Mesa/llvmpipe context**
(Xvfb, no physical GPU).

!!! warning "Important limitation: no real GPU pipeline"
    On llvmpipe, there is **no asynchronous GPU pipeline** — everything
    executes synchronously on the CPU. The CPU↔GPU sync stall that orphaning
    eliminates **does not exist** in this context. The timings essentially
    measure `memcpy` cost + software driver overhead, not a real sync stall.

Concretely:

1. The absolute timings (µs) are **not representative of real GPU hardware**.
   A discrete NVIDIA/AMD GPU will have radically different characteristics
   (asynchronous DMA, hardware buffer pool, PCIe latency).
2. The **relative** timings are still useful for detecting **code regressions**:
   if a refactoring accidentally adds an O(N³) or an extra memcpy, the CI
   baseline will catch it.
3. This baseline **does NOT measure orphaning effectiveness** — it cannot
   distinguish orphan vs non-orphan on llvmpipe.

## A/B Results on Real Hardware

Measured with `just bench-ab` on **Intel Iris Xe (RPL-U), Mesa 25.0.7-2**,
3 consecutive runs of 5 iterations each:

| Metric | Run 1 | Run 2 | Run 3 | Average |
|--------|-------|-------|-------|---------|
| `draw (build+upload+render)` | **-52.6%** | **-39.3%** | **-44.7%** | **~-45%** |
| `update+upload` | -8.7% | +2.6% | -9.1% | **~0%** (noise) |

### Analysis

**Trail renderer (`draw`): stable ~45% improvement.** The orphaning pattern
eliminates a real synchronization stall on Mesa/Intel. Without orphaning,
`glBufferSubData` must wait for the GPU to finish reading the old buffer before
writing the new one. With `glBufferData(NULL)` + `glBufferSubData`, Mesa
allocates a new backing store immediately — no stall. The high stddev on master
(193–682 µs) confirms sporadic stalls that disappear with orphaning.

**Instance update (`update+upload`): no measurable gain.** The instance data is
small (~50 KB for 14 bodies × `sizeof(SphereInstance)`), not enough to provoke
a measurable stall. The ±5% variation is statistical noise.

**Key takeaway:** buffer orphaning is most effective on large, frequently
rebuilt buffers. The trail renderer's ~527 KB per-frame rebuild is the primary
beneficiary. Instance data is too small to stall noticeably on modern drivers.

!!! note "Driver-dependent behavior"
    Some drivers (e.g., NVIDIA proprietary) may perform implicit orphaning
    internally, making the explicit pattern redundant. Mesa/Intel does not,
    which is why the gain is significant on this hardware.

### Usage for Optimization (Real GPU)

The primary purpose of this benchmark is to **guide the NBody pipeline
optimization work**. For this, it must be run on the development machine
with the real GPU:

```bash
# Direct execution with real GPU (no Xvfb)
./build/tests/test_benchmark_buffer_upload
```

On a real GPU, the benchmark captures actual synchronization stalls:

- **Orphaning vs without comparison**: checkout master (no orphaning),
  measure → checkout branch (with orphaning), measure → compare the `Avg`.
  The delta represents the eliminated stall time.
- **Evaluating future optimizations**: double-buffering, persistent mapping,
  compute ribbons — each approach is measured with the same tool.
- **Max profile**: on real GPU, a high `Max` indicates a real synchronization
  stall (GPU hadn't finished reading the buffer), not just OS noise.

| Context | What is measured | Utility |
|---------|-----------------|----------|
| **Xvfb/llvmpipe** (CI) | memcpy + software driver overhead | Code regression detection |
| **Real GPU** (dev) | Actual CPU↔GPU stall + DMA upload | Optimization validation, A/B comparison |

## How to Run

### Quick Single Run (real GPU)

```bash
# Via Just recipe — builds if needed, runs directly (no Xvfb)
just bench-nbody

# Or run the binary directly
./build/tests/test_benchmark_buffer_upload
```

### Run in CI / Test Suite (Xvfb)

```bash
# Verbose output via CTest (uses Xvfb wrapper)
cd build && ctest -R test_benchmark_buffer_upload -V

# As part of the full test suite
just test-all
```

## How to Use the Baseline

### Automated A/B Comparison Between Branches

The `just bench-ab` recipe automates the full A/B comparison workflow:

```bash
# Compare current branch vs master (5 runs per side, default)
just bench-ab

# Compare against a specific branch with custom run count
just bench-ab origin/main 10
```

The recipe:

1. Verifies the working tree is clean (commit or stash first)
2. Builds and runs the benchmark N times on the current branch
3. Switches to the reference branch (auto cherry-picks all benchmark test
   commits if the test doesn't exist on that branch)
4. Builds and runs the benchmark N times on the reference
5. Switches back to the original branch
6. Displays the GL renderer and version used for measurement
7. Computes mean ± stddev for each metric and prints a comparison table:

```text
Metric                            current-branch         master       Delta    Change
──────────────────────────────  ───────────────  ───────────────  ──────────  ────────
update+upload                      500.1±45.1      517.5±19.9  -17.4 µs  ▼-3.4%
draw (build+upload+render)        1029.8±82.1      894.0±55.0  135.8 µs  ▲15.2%
```

- **▼ green** = current branch is faster (improvement)
- **▲ red** = current branch is slower (regression)

!!! tip "Real GPU required for meaningful results"
    Run `just bench-ab` from a terminal with access to a real GPU.
    Do **not** run under Xvfb — the results would only measure software
    rendering overhead, not actual CPU↔GPU synchronization stalls.

#### A/B Script Features

- **Build caching**: the reference branch builds into `build-bench-ref/`
  (separate from the working `build/`). This directory persists across runs,
  so subsequent A/B comparisons only rebuild changed files (incremental).
- **GL renderer detection**: prints the GPU driver and version at the top
  of the results. Warns if a software renderer (llvmpipe/softpipe) is
  detected, or if the two sides used different renderers.
- **Multi-commit cherry-pick**: when the benchmark test doesn't exist on
  the reference branch, the script cherry-picks ALL commits touching the
  test file (not just the initial add), ensuring features like GL renderer
  printing are included.
- **Automatic cleanup**: temporary cherry-picks are never committed; the
  script restores the reference branch to its original state.

### Manual A/B (alternative)

```bash
# 1. Run benchmark on current branch, save output
just bench-nbody 2>&1 | tee /tmp/bench_current.txt

# 2. Switch to comparison branch and rebuild
git checkout master && just build

# 3. Run benchmark again
just bench-nbody 2>&1 | tee /tmp/bench_master.txt

# 4. Compare the "Avg" lines
diff <(grep "Avg" /tmp/bench_master.txt) <(grep "Avg" /tmp/bench_current.txt)
```

### CI Regression Guard

The benchmark is registered in `tests/CMakeLists.txt` as part of the
`OPENGL_TESTS` list and runs under Xvfb in CI. To add a hard regression
threshold, wrap the test with a CTest timeout or add assertions:

```c
// Example: fail if average exceeds 5ms (would indicate a sync stall)
TEST_ASSERT_MESSAGE(avg_us < 5000.0,
    "Instance update exceeds 5ms — possible sync regression");
```

This is intentionally **not** enabled yet because the absolute threshold depends
on the CI runner hardware. Once baseline stability is confirmed across multiple
CI runs, a threshold can be calibrated.

### Tracking Over Time

Record benchmark results in a simple log to track trends:

```bash
echo "$(git rev-parse --short HEAD) $(date +%F) $(ctest -R test_benchmark_buffer_upload -V 2>&1 | grep 'Avg')" >> perf_log.txt
```

## Maintenance

### When to Update the Benchmark

- **New dynamic buffer path**: if a new subsystem does per-frame VBO uploads
  (e.g., particle system, cloth simulation), add a benchmark test following
  the same pattern.
- **API change**: if `instanced_group_update` or `trail_renderer_draw` change
  signature, update the benchmark calls accordingly (the compiler will catch
  most breakages).
- **Parameter change**: if `NBODY_MAX_BODIES` or `TRAIL_MAX_POINTS` change,
  the benchmark automatically adapts (it reads these from headers).

### What NOT to Change

- **Frame count and warmup**: changing `FRAMES` or `WARMUP_FRAMES` invalidates
  comparison with previous baselines. If changed, document the reason and
  re-establish the baseline.
- **Timer function**: `clock_gettime(CLOCK_MONOTONIC)` is the reference clock.
  Do not switch to `glfwGetTime` (lower resolution) or `rdtsc` (not portable).

## Relationship to Other Benchmarks

| Benchmark | Scope | Timer | Location |
|-----------|-------|-------|----------|
| **This (buffer upload)** | CPU-side upload latency for NBody | `clock_gettime` (CPU wall-clock) | `tests/test_benchmark_buffer_upload.c` |
| [Effect Benchmark](effect_benchmark.md) | GPU cost of individual post-process effects | GPU timestamps (`glQueryCounter`) | `src/effect_benchmark.c` |
| [Perf Benchmarking Protocol](perf_benchmarking_protocol.md) | Full application GPU profiling (multi-run, statistical) | GPU timestamps + `AdaptiveSampler` | `scripts/perf_benchmark.py` |
| [Billboard Perf](billboard_optimization.md) | Billboard rendering throughput | CPU wall-clock | `tests/test_billboard_perf.c` |

## References

- [NBody Buffer Upload Optimization](nbody_buffer_optimization.md) — the optimization this benchmark validates
- [OpenGL Wiki — Buffer Object Streaming](https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming)
- [Performance Benchmarking Protocol](perf_benchmarking_protocol.md) — project-wide benchmarking methodology
- [Effect Benchmark](effect_benchmark.md) — GPU-side effect cost measurement
