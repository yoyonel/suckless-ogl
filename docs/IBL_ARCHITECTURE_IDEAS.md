# IBL Optimization Strategies

This document outlines architectural ideas for reducing the performance impact of environment map updates, specifically the long GPU stalls during Prefiltered Specular Map generation.

## 1. Problem Analysis
Current metrics show that generating the Prefiltered Specular Map for a 4K environment takes ~350ms on integrated graphics. This causes a complete freeze of the rendering loop.

## 2. Proposed Solutions

### A. Progressive IBL (Temporal Amortization)
Instead of calculating all mipmap levels in a single loop, process them incrementally.
- **Mechanism**: Use a state machine in the `AsyncLoader` or `App` loop.
- **Workflow**: `Frame N: Load HDR` -> `Frame N+1: Mip 0` -> `Frame N+2: Mip 1` ...
- **Result**: Zero freezes. The user sees the environment "sharpen" or "blur" over a few frames.

### B. Adaptive Sample Counting
Lower mip levels represent rougher surfaces where high-frequency details are already lost.
- **Idea**: Use 1024 samples for Mip 0, but scale down to 512, 256, 128 for higher mips.
- **Technique**: Pass `u_sample_count` as a uniform to the compute shader.

### C. Resolution Capping
Directly processing a 4K texture for IBL is often overkill as reflexes are rarely pixel-perfect.
- **Rule**: Cap the Prefiltered Specular Map resolution to **1024x512** regardless of the source HDR size.
- **Benefit**: Quadrant reduction in complexity compared to 4K.

### D. Tiled Dispatching
For very large mips, even a single `glDispatchCompute` can exceed a reasonable frame budget.
- **Idea**: Split a single mipmap level into tiles (e.g., 4x4 grid).
- **Mechanism**: Dispatch only a subset of tiles per frame until the level is complete.

### E. Instant Placeholder (The "Fast Path")
Avoid waiting for any compute shader to show the new environment.
- **Workflow**:
    1. Load HDR to RAM.
    2. Upload to VRAM.
    3. `glCopyImageSubData` HDR to SpecMap Mip 0 (Instant).
    4. Gradually replace mips as they are computed.
- **Result**: Visual feedback is instantaneous.

## 3. Implementation Roadmap
1. [ ] Implement **Resolution Capping** (Easiest, high ROI).
2. [ ] Implement **Progressive Mip Generation** (Best user experience).
3. [ ] Implement **Adaptive Samples** (Pure GPU optimization).
