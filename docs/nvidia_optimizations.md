# NVIDIA OpenGL Support: Launch, Stability & Optimizations

This document details the issues encountered when running the application on NVIDIA hardware and the solutions implemented to ensure a stable startup and high-performance execution.

---

## I. Startup & Stability Fixes (Critical)

These issues prevented the application from launching or caused immediate crashes/invalid states on NVIDIA drivers.

### 1. Robust Texture Mipmap Allocation (Error 0x501)
- **Problem**: The application could fail or render incorrectly during IBL map generation. NVIDIA drivers are strict about `glTexStorage2D` parameters. An incorrect `levels` calculation for high-resolution 4K textures triggered `GL_INVALID_VALUE` (0x501).
- **Fix**: Implemented a precise mip-level formula: `1 + (int)floor(log2(fmax(width, height)))` and added defensive range checks.
- **Result**: Immediate resolution of 0x501 errors during the asynchronous loading phase.

### 2. Object Labeling & Initialization Sequence (Error 1282)
- **Problem**: Calling `glObjectLabel` on objects immediately after `glGen*` caused "Unknown Object" errors. NVIDIA considers objects "uninitialized" until they are bound.
- **Fix**: Reordered the sequence to ensure every object (VAO, VBO, Texture) is **bound at least once** before labeling.
- **Result**: Clean debug logs and stable startup sequence.

---

## II. Performance & Warning Cleanups

These changes addressed recurring performance warnings and optimized the rendering pipeline for NVIDIA's driver heuristics.

### 1. Dummy Texture Strategy (Unit Binding Cleanup)
- **Issue**: Binding `0` to an active texture unit triggered "undefined base level" warnings and potential driver-side shader recompilations.
- **Fix**: Introduced `dummy_black_tex` and `dummy_white_tex` (1x1 pixels). These are bound whenever a real texture (like HDR environment or Bloom) is pending or disabled.
- **Result**: Perfectly clean debug logs even during asynchronous asset loading; stable shader execution.

### 2. VAO State Reconciliation (Shader Recompilation)
- **Issue**: Frequent "Vertex shader recompiled based on GL state" (0x20092) warnings caused by indeterminate attribute divisor states across different VAOs.
- **Fix**:
    - Explicitly called `glVertexAttribDivisor(index, 0)` for all non-instanced attributes in all VAOs.
    - Systematically disabled unused attribute arrays (indices 1-7) in specialized VAOs (Skybox, Billboards).
- **Result**: Recompilation is now limited to a one-time startup JIT event, eliminating runtime stuttering.

### 3. Efficient Memory Placement (Buffer Movement)
- **Issue**: Performance warnings (0x20072) regarding buffer migration between VIDEO and HOST memory during luminance reduction transfers.
- **Fix**:
    - **Caching**: Luminance SSBOs are now persistent in the `App` struct to avoid per-frame allocation overhead.
    - **Buffer Storage**: Used `glBufferStorage` with `GL_MAP_READ_BIT | GL_CLIENT_STORAGE_BIT` to hint to the driver that these small readback buffers should stay in host-visible memory.
    - **Fast Readback**: Replaced mapping with `glGetBufferSubData` to minimize synchronization stalls for single-float transfers.
- **Result**: Smooth asynchronous exposure adaptation without driver-staged copies.
