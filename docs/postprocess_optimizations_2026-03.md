# Technical Analysis: Post-Process Optimizations (March 2026)

This document details GPU optimization paths for the post-processing pipeline of `suckless-ogl`, based on migrating computations to **Compute Shaders**.

## General Objectives

1. **Reduce CPU overhead**: Eliminate Framebuffer (FBO) switches and multiple draw calls.

2. **Maximize GPU occupancy**: Leverage the massive parallelism of compute units (EUs/CUs) via Compute Shaders.

3. **Reduce synchronization barriers**: Minimize waits between passes.

---

## Part 1: Auto-Exposure (Luminance Calculation)

### Concept

Replace the rasterization pass (Fragment Shader on a 64x64 quad) with a Compute Shader processing the scene texture.

### Critical Points (Lessons Learned)

To maintain **ISO parity with master**, the Compute Shader must strictly replicate the physical logic:

- **4x4 Sampling**: Do not settle for a single `texture()` at the center. Average a pixel block (box filter) to capture light peaks.

- **Exclusion Threshold (0.05)**: Ignore pixels with luminance below 0.05. Without this, black sky or deep shadows pull the average down, causing massive overexposure.

- **Sentinel Value (-100.0)**: If a block is entirely black, it must be marked for the adaptation step to ignore it.

### Implementation Steps

1. **Shader**: Create `shaders/lum_downsample.comp` with a 4x4 sampling loop.

2. **Texture**: Switch `downsample_tex` to `R32F` format for image storage (`image2D`).

3. **C Code**: Remove `downsample_fbo`. Replace `glDrawArrays` with `glDispatchCompute(8, 8, 1)`.

4. **Barrier**: Add `glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)` before the adaptation step.

> [!TIP]
> Pour une analyse détaillée des gains et du protocole de mesure, consultez le [Rapport d'Optimisation Auto-Exposure](auto_exposure_opt_report.md).

---

## Part 2: Bloom (Single-Pass Downsampling)

### Concept

Replace the 5–6 successive downsampling passes with a single Compute Shader dispatch (similar to AMD's _Single Pass Downsampler_).

### Technical Details

- **Mip Hierarchy**: Use `glBindImageTexture` to bind multiple mipmap levels (1 to 4) simultaneously.

- **Parallelism**: Each work group (8x8) processes a region and writes to the corresponding mips.

- **Format**: Use `R11F_G11F_B10F` for compact, performant HDR storage.

### Implementation Steps

1. **Shader**: Create `shaders/bloom_downsample.comp`.

2. **C Code**: Modify `fx_bloom_init` to configure mip textures with image access.

3. **Render**: Replace the fragment render loop with a single dispatch. Keep Raster mode for Upsampling (which benefits from hardware blending `GL_ONE, GL_ONE`).

---

## Part 3: Resource Management & RAII

### Concept

Ensure reliable resource deletion during engine restarts or resolution changes.

### Recommendations

- **`SHADER_SAFE_DESTROY` Macro**: Always use a macro that checks for null before calling `shader_destroy`.

- **FBO Cleanup**: Ensure that textures attached to FBOs are freed _after_ the FBOs to avoid dangling pointers in the driver.

---

## Part 4: Validation and Metrics

### Test Methodology

1. **Visual Parity**: Use `tests/test_visual_fx.c` to compare Raster and Compute output pixel-by-pixel. Any mean luminance difference > 1% must be treated as a bug.

2. **Benchmarking**: Use `apitrace` to verify the elimination of "bubbles" in the pipeline (zones where the GPU waits for the CPU).

3. **Watchdog**: For tests under Wine, implement an exit timeout if the application ignores the `Escape` signal due to X11 desynchronization.

---

_Analysis performed on March 13, 2026._
