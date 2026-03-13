# SIMD Optimization: Texture Uploads

## Overview

This guide details the implementation of SIMD (Single Instruction, Multiple Data) optimizations used to accelerate the conversion of high-dynamic-range (HDR) texture data from 32-bit floating point (`float`) to 16-bit half-precision (`half`) format.

## Motivation

Profiling via [Tracy](profiling_tracy.md) revealed that `glTexSubImage2D` was a significant bottleneck during the loading phase, particularly for large HDR environment maps (4K resolution).

- **Problem**: The OpenGL driver was performing the `Float32 -> Float16` conversion on the CPU using a scalar (single-value) path, leading to high CPU usage and stalling the main thread. See the detailed [Mesa F32-to-F16 Analysis](mesa_f32_to_f16_analysis.md) for a technical breakdown of this bottleneck.

- **Goal**: Offload this conversion to optimized hardware vector instructions (AVX2/F16C) before handing the data to the driver, allowing `glTexSubImage2D` to perform a fast `memcpy`.

## Implementation

The optimization is implemented in `src/simd_utils.c` and relies on x86-64 extensions:

- **AVX2**: For processing 8 floats (256 bits) in parallel.

- **F16C**: For the `_mm256_cvtps_ph` intrinsic, which converts a vector of 8 floats to 8 half-floats.

### Code Path

1. **Check**: The build system determines if `__AVX2__` and `__F16C__` are available.

2. **SIMD Path**: If available, the code processes pixels in batches of 8 using intrinsics.

3. **Scalar Fallback**: For the remaining pixels ("tail") or on unsupported hardware, a scalar implementation `float_to_half_intrinsic` is used.

### Build System Configuration

To ensure these instructions are used, the build system (`CMakeLists.txt`) forces the use of native architecture flags when `ENABLE_NATIVE_ARCH=ON` is set.

```cmake
option(ENABLE_NATIVE_ARCH "Enable native architecture optimizations" ON)
if(ENABLE_NATIVE_ARCH)
    add_compile_options(-march=native -mavx2 -mf16c)
endif()

```

> [!IMPORTANT]
> This requires the host CPU (where the code is compiled) to support these instructions. The CI/CD pipeline or distrobox container must expose these CPU flags.

## Performance Analysis

### Impact

- **Before**: 4K HDR upload took **~112ms** (driver conversion).

- **After**: 4K HDR upload takes **~35ms** (SIMD conversion + upload).

- **Speedup**: **~3.2x** faster texture uploads.

### Memory Trade-off

This optimization requires a temporary buffer to store the converted 16-bit data before upload.

- **Ram Usage**: `Width * Height * 4 (RGBA) * 2 (Bytes)`

- **Example**: A 4K texture (4096x2048) requires a **~64MB** temporary allocation.

- **Rationale**: We trade transient RAM usage for significantly reduced CPU time to avoid frame stutters during loading.

## Verification

To verify the optimization is active at runtime, check the log output:

```text
[INFO] simd_utils: SIMD Optimization: AVX2/F16C Enabled

```

If you see `Software Fallback (No F16C/AVX)`, check your build flags and CPU capabilities (`lscpu | grep f16c`).
