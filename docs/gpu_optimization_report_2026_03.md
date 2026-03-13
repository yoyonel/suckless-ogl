# GPU Optimization Report - March 2026

## 1. Executive Summary

This report details the performance gains achieved through the implementation of **Compute-based Post-Processing** and **Pass Fusion** in the `suckless-ogl` renderer.

- **Total GPU Frame Time Reduction**: **~21%** (from 3.08ms to ~2.45ms).
- **Post-Process Draw Call Reduction**: **-82%** (from ~17 to 3).
- **Key Optimizations**: Bloom Single-Pass Downsampler (SPD) & Auto-Exposure Fusion.

## 2. Baseline vs. Optimized (GPU Metrics)

Measurements performed on **Intel Iris Xe (iGPU)** at 1080p resolution.

| Stage | Baseline (Fragment-based) | Optimized (Compute + Fusion) | Gain |
| :--- | :--- | :--- | :--- |
| **Auto-Exposure** | 0.69 ms | **0.01 ms** | **-98%** (Redundant pass removed) |
| **Bloom** | 0.19 ms | **0.24 ms** | +0.05 ms (Now handles AE reduction) |
| **Final Composite** | 0.47 ms | **0.47 ms** | Stable (No regression) |
| **Total Post-Process** | **1.60 ms** | **0.97 ms** | **-40%** |
| **Draw Calls (PP)** | **~17** | **3** | **-14 Draw Calls** |

## 3. Detailed Optimizations

### A. Bloom Single-Pass Downsampler (SPD)
Instead of 6 fragment shader passes (one per mip level), we now use a single **Compute Shader** (`bloom_downsample.comp`).
- **Technical Gain**: Utilizes **GPU Shared Memory** (LDS) to perform the reduction in one dispatch.
- **Hardware Impact**: Drastically reduces VRAM bandwidth usage, which is the primary bottleneck on integrated GPUs (Intel Iris Xe).

### B. Auto-Exposure Fusion
The luminance reduction for Auto-Exposure was previously a separate raster pass. It is now **integrated directly** into the Bloom SPD Compute Shader.
- **Technical Gain**: The Bloom shader already samples the entire screen. By calculating the average luminance in the same pass, we eliminate a complete read/write cycle of the scene texture.
- **Performance Impact**: Effectively makes the Auto-Exposure downsampling cost **zero**.

## 4. Hardware-Specific Insights

- **Intel Iris Xe (iGPU)**: Benefited most from the bandwidth reduction. The frame is much more stable, with fewer micro-stutters during heavy post-process toggles.
- **NVIDIA 950M (dGPU/Maxwell)**: The reduction of 14 draw calls significantly lowered the CPU overhead, allowing for a more consistent 60 FPS target even on this aging hardware.

## 5. Next Steps

To reach sub-2.0ms frame times, the next logical steps are:
1. **Pass Merging**: Fusing Tonemapping, Color Grading, and FXAA into a single compute pass.
2. **Depth Pre-Pass**: Implementing an Early-Z pass to reduce PBR shader cost in complex scenes.

---
*Generated via `just bench-all` and `trace_analyze.py` on March 13, 2026.*
