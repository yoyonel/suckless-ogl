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

### Revised Hypothesis — CPU-GPU Pipeline Bubble (Confidence: ~~95%~~ → **Invalidated**)

The initial Tracy timeline analysis suggested a CPU-GPU pipeline bubble:

```text
GPU: [==Scene+PP+UI==][.........Swap Buffers (IDLE).........][==next frame==]
CPU: [==GL commands==][Tracy][Swap][Collect][Poll][Update][==GL commands==]
                       <---- GPU idle while CPU does non-GL work ---->
```

**This hypothesis was tested and invalidated.** Reordering the main loop (moving CPU-only work after SwapBuffers) produced no measurable improvement — the GPU idle gap remained identical. The "Swap Buffers" GPU idle was not caused by a pipeline bubble but by external throttling (see H6).

### Final Root Cause — X11/Compiz Compositor Overhead (Confidence: 95%)

Deeper Tracy profiling in fullscreen mode revealed the **actual root cause**:

**Evidence from Tracy (Frame 2,053, fullscreen, VSync OFF):**

| Zone | Time | % of Frame |
|:---|:---:|:---:|
| `GLFW PollEvents` | **625 µs** | **28%** |
| GPU useful work | ~200 µs | 9% |
| Render submit + Swap | ~400 µs | 18% |
| GPU idle (starving) | ~1000 µs | **45%** |

`glfwPollEvents()` calls into the X11 server, which is mediated by the Compiz compositing window manager. Each round-trip incurs significant latency compared to Wayland's direct model.

**Cross-platform comparison:**

| Environment | Display | Compositor | `PollEvents` | GPU Usage |
|:---|:---|:---|:---:|:---:|
| Intel Iris Xe (i7-1355U) | X11 | Compiz | ~625 µs | ~63% |
| NVIDIA 950M (Bazzite) | Wayland | Native | ~10-50 µs | ~99% |

The GPU is idle ~37% of the time because the CPU spends 625 µs per frame in X11/Compiz event polling — time during which no GL commands are submitted. The NVIDIA 950M achieves 100% not because it's slower, but because Wayland's event model has negligible overhead.

**This is a system-level limitation, not an application-level bug.** No code change can reduce X11/Compiz `glfwPollEvents()` latency.

### Updated Confidence Table

| # | Finding | Impact | Confidence | Method |
|:---:|:---|:---:|:---:|:---|
| ~~H1~~ | Query readback stall | ~37 µs (negligible) | **Measured** | Tracy Statistics |
| ~~H2~~ | Sort barrier flushes | ~55 µs (negligible) | **Measured** | Tracy Statistics |
| ~~H3~~ | UBO implicit sync | < 10 µs (negligible) | **Measured** | Tracy Statistics |
| H4 | Shared memory bandwidth | Structural, not primary | 40% | Unchanged |
| ~~H5~~ | ~~CPU-GPU pipeline bubble~~ | ~~30-40% GPU idle~~ | **Invalidated** | Loop reorder tested, no effect |
| **H6** | **X11/Compiz event polling overhead** | **~28% frame time, ~37% GPU idle** | **95%** | **Tracy Timeline (fullscreen)** |

## Proposed Fix — Main Loop Reordering (**Tested — No Effect**)

The loop reordering approach was implemented and tested:

```text
Before: PollEvents → physics/camera → App Update → Render → Tracy → SwapBuffers → Collect
After:  PollEvents → Render → SwapBuffers → physics/camera/App Update → Collect
```

**Result:** No measurable change in GPU utilization or frame time. The reorder was reverted because:

1. It added 1 frame of input latency for zero benefit
2. The GPU idle gap was caused by X11/Compiz, not by CPU-side work ordering

## Conclusion

The ~63% GPU utilization on Intel Iris Xe under X11/Compiz is a **system-level characteristic**, not an application defect. The GPU renders the scene in ~200 µs but the CPU spends ~625 µs in X11 event polling per frame, starving the GPU of new work.

**Mitigation options (all external to the application):**

| Option | Expected Impact | Feasibility |
|:---|:---|:---|
| Switch to Wayland compositor | GPU → ~100% | Requires desktop environment change |
| Use a non-compositing WM (e.g., i3, dwm) | Reduced PollEvents overhead | User preference |
| Disable Compiz compositing (`compiz --replace --no-composite`) | Partial improvement | May break desktop features |

No application-level code changes are planned for this issue.

## Phase 1: Tracy Instrumentation (Done)

Added `PROFILE_ZONE` CPU markers at key synchronization points:

| Zone | File | Purpose |
|:---|:---|:---|
| `"GPU Query Readback (sync)"` | `gpu_profiler.c` | Measure blocking `glGetQueryObjectui64v` loop |
| `"GI Probe Sync (buffer upload)"` | `scene.c` | `glBufferSubData` SSBO + 3D texture packing for GI probes |
| `"GPU Sort: SSBO Upload"` | `billboard_sorter.c` | Instance data transfer to GPU |
| `"GPU Sort: Compute Dispatch"` | `billboard_sorter.c` | Full dispatch + barrier chain |
| `"PostProcess UBO Upload"` | `postprocess.c` | `glBufferSubData` implicit sync detection |

## Phase 2: Main Loop Reordering (**Tested — Invalidated**)

Reorganized `app_run()` to move CPU-only work (camera physics, UI update, notifier, sampler) after `glfwSwapBuffers()`. The change compiled and passed all 60/60 tests but produced **zero improvement** in GPU utilization. The reorder was reverted.

This led to the discovery of the actual root cause (H6: X11/Compiz overhead) through additional Tracy instrumentation of the full frame loop.

### Additional Tracy Zones (Phase 2 Diagnostic)

| Zone | File | Purpose |
|:---|:---|:---|
| `"Frame Timing"` | `app.c` | Timing/FPS/sampler block |
| `"UI & Notifier Update"` | `app.c` | Action notifier, UI overlay, postprocess time |
| `"Camera Physics"` | `app.c` | Fixed timestep physics + rotation interpolation |
| `"PostProcess Resize"` | `app.c` | Deferred FBO/texture recreation |
| `"Icosphere Regen"` | `app.c` | Mesh regeneration on subdivision change |
