# Visual Testing & Regression Artifacts

This document explains the technical reasons behind visual differences observed in automated regression reports, particularly between local hardware rendering and CI software rendering.

## The Core Challenge: Software vs. Hardware

The project CI (GitHub Actions) uses **Mesa llvmpipe**, a software-based OpenGL implementation that runs on the CPU. While highly compliant, it differs from hardware GPUs (NVIDIA, Intel, AMD) in several ways:

### 1. Floating Point Precision

* **Bit-Level Parity**: Even with standard-compliant drivers, transcendental functions like `pow()`, `exp()`, `sin()`, and `cos()` vary slightly in their low-level implementation.
* **Accumulated Errors**: In complex PBR shaders, these micro-discrepancies accumulate, leading to "salt and pepper" noise in difference maps.

### 2. The "Sphere Center" Artifact

A common finding in our PBR tests is a concentric ring pattern at the center of spheres.

* **Cause**: At the center of a sphere, the dot product **N · V** is exactly `1.0`.
* **Sensitivity**: The BRDF LUT (Look-Up Table) is sampled using (**N · V**, **roughness**). Any tiny floating-point variation in the calculation of **N · V** or the texture sampling coordinates causes the hardware to pick a different pixel in the LUT than the software renderer.
* **Result**: High-contrast deltas in the difference map, even if the visual change is imperceptible to the human eye.

## PBR Engine Evolution

Recent improvements to the rendering engine also contribute to deltas compared to older "Master" references:

### 1. Multiple Scattering (Kulla-Conty)

We implemented a compensation term for energy loss on rough surfaces. This increases the brightness of metallic/rough materials, changing the overall luminance compared to a "Standard" PBR implementation.

### 2. Analytic Roughness Clamping

To ensure portability across vendors, we replaced derivative-based smoothing (`fwidth`) with **Analytic Roughness Clamping** (`MIN_ROUGHNESS = 0.03`).

* **Benefit**: Identical results on Intel, NVIDIA, and Mesa.
* **Delta**: Reference images captured with older versions using `fwidth` will show significant edges deltas.

## Guidelines for Reference Updates

When a Visual Regression Report shows failures:

1. **Check the Difference Map**: If the deltas are concentrated at geometric edges or in smooth gradients (PBR centers), it is likely a precision delta.
2. **Verify PR Intent**: If the PR modified PBR math or synchronization, a delta is expected.
3. **Update Reference**: If the PR is visually correct but fails the automated threshold due to the reasons above, update the references in `tests/ref_*.png`.
    * Run `GEN_REFS=1 tests/run_test_with_xvfb.sh build/tests/test_app` to regenerate all 6 faces locally.
    * Commit the updated PNG files.

## Multi-View (Cube) Regression

To ensure full scene consistency, the engine captures 6 viewpoints around the origin `(0,0,0)`:

* **Faces**: `front`, `back`, `left`, `right`, `top`, `bottom`.
* **Camera Distance**: Fixed at `25.0` to cover the entire setup.
* **Bootstrapping**: The system uses a specialized loop in `test_app.c` that can either compare frames (test mode) or write them (generation mode).

---
*See also: [Shader Cross-GPU Compatibility](./shader-cross-gpu-compatibility.md)*
