# GPU Utilization Optimization

Goal: maximize GPU utilization toward 100% on the primary dev environment (Intel Iris Xe, i7-1355U).

## Baseline Measurements (2026-04-03)

| Metric | Intel Iris Xe (iGPU) | NVIDIA 950M (dGPU) |
|:---|:---:|:---:|
| **GPU Usage (MangoHud)** | ~63% | ~99% |
| **FPS** | 154 | 129 |
| **Total Frame** | 6.85 ms | 7.75 ms |
| **Scene Render** | 2.25 ms | 3.29 ms |
| **Billboard Render** | 1.31 ms | 2.55 ms |
| **Post-Process** | 3.20 ms | 3.39 ms |
| **Swap Buffers** | 1.16 ms | 0.87 ms |

## Analysis Evolution

### Initial Hypotheses (Pre-Tracy)

| # | Hypothesis | Estimated Impact | Confidence |
|:---:|:---|:---:|:---:|
| H1 | GPU Profiler query readback (`glGetQueryObjectui64v`) blocks CPU | 15-25% GPU idle | 70% |
| H2 | Bitonic sort GPU barriers cause pipeline flushes | 2-3% | 50% |
| H3 | PostProcess UBO `glBufferSubData` implicit sync on Mesa | 3-5% | 40% |
| H4 | Shared memory bandwidth (iGPU) limits throughput | structural | 60% |

### Tracy Instrumentation Results (Measured)

Added `PROFILE_ZONE` markers around key sync points. Tracy Statistics revealed:

| Zone | Mean | Median | P99 | Verdict |
|:---|:---:|:---:|:---:|:---|
| GPU Query Readback (sync) | 37 µs | 33 µs | 94 µs | **Not a bottleneck** — negligible |
| GPU Sort: SSBO Upload | 28 µs | — | — | **Not a bottleneck** |
| GPU Sort: Compute Dispatch | 55 µs | 50 µs | 135 µs | **Not a bottleneck** |
| PostProcess UBO Upload | (< 10 µs) | — | — | **Not a bottleneck** |

**All four initial hypotheses were invalidated by measurement.** None of these sync points cause significant stalls.

### Revised Hypothesis — CPU-GPU Pipeline Bubble (Confidence: 95%)

Tracy timeline analysis revealed the **actual root cause**:

```text
GPU: [==Scene+PP+UI==][.........Swap Buffers (IDLE).........][==next frame==]
CPU: [==GL commands==][Tracy][Swap][Collect][Poll][Update][==GL commands==]
                       <---- GPU idle while CPU does non-GL work ---->
```

**Evidence from Tracy (Frame 3,392):**

- Total Frame CPU execution: **3.21 ms** (self time: 52 µs = 1.63%)
- GPU useful work: ~2.0–2.5 ms per frame
- GPU idle gap (visible as "Swap Buffers" in GPU lane): ~1.0–1.5 ms
- GPU idle time / total frame ≈ **30-40%** → matches MangoHud ~63% utilization

**The problem is structural**: the main loop executes CPU-only work (physics, camera, matrix computation, Tracy housekeeping, event polling) **after** all GL commands are submitted and **before** submitting the next frame's commands. The GPU finishes its work and starves.

### Updated Confidence Table

| # | Finding | Impact | Confidence | Method |
|:---:|:---|:---:|:---:|:---|
| ~~H1~~ | Query readback stall | ~37 µs (negligible) | **Measured** | Tracy Statistics |
| ~~H2~~ | Sort barrier flushes | ~55 µs (negligible) | **Measured** | Tracy Statistics |
| ~~H3~~ | UBO implicit sync | < 10 µs (negligible) | **Measured** | Tracy Statistics |
| H4 | Shared memory bandwidth | Structural, not primary | 40% | Unchanged |
| **H5** | **CPU-GPU pipeline bubble (main loop ordering)** | **~30-40% GPU idle** | **95%** | **Tracy Timeline** |

## Proposed Fix — Main Loop Reordering

Currently (`app_run()` in `app.c`):

```text
PollEvents → physics/camera → App Update → Render (GL cmds) → Tracy → SwapBuffers → Collect
```

Proposed:

```text
PollEvents → Render (GL cmds) → SwapBuffers → physics/camera/App Update → Collect
```

By moving CPU-only work **after** SwapBuffers, the CPU prepares frame N+1 **while** the GPU executes frame N. The pipeline bubble disappears.

**Risks:**

- Camera/physics data will be **one frame old** relative to rendering (adds 1 frame of input latency). At 154 FPS this is ~6.5 ms — acceptable for this use case.
- Some updates (resize, async loading) may need careful ordering.

## Phase 1: Tracy Instrumentation (Done)

Added `PROFILE_ZONE` CPU markers at key synchronization points:

| Zone | File | Purpose |
|:---|:---|:---|
| `"GPU Query Readback (sync)"` | `gpu_profiler.c` | Measure blocking `glGetQueryObjectui64v` loop |
| `"GI Probe Sync (buffer upload)"` | `scene.c` | `glBufferSubData` SSBO + 3D texture packing for GI probes |
| `"GPU Sort: SSBO Upload"` | `sphere_sorting.c` | Instance data transfer to GPU |
| `"GPU Sort: Compute Dispatch"` | `sphere_sorting.c` | Full dispatch + barrier chain |
| `"PostProcess UBO Upload"` | `postprocess.c` | `glBufferSubData` implicit sync detection |

## Phase 2: Main Loop Reordering (Planned)

Reorganize `app_run()` to overlap CPU work with GPU execution.
