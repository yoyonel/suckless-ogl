# UI Performance Analysis (GPU Cost)

This document analyzes the GPU performance hit reported by the user (> 2ms for the keyboard overlay) and proposes optimizations.

## 1. Problem Identification: Draw Call Explosion

The current rendering architecture in `app_ui.c` and `ui.c` suffers from a **state-change bottleneck**. The keyboard overlay consists of roughly 70 keys, each composed of several layers:

- **Base Texture** (`kbd_key_base.png`)
- **Bloom Glow** (`kbd_key_glow.png`) - *Requires Additive Blending State*
- **SDF Border** (Procedural)
- **Key Label** (Font Atlas Texture)

### The "Ping-Pong" Effect

In `draw_help_overlay_keys`, we iterate through each key and draw all its layers before moving to the next key. Because each layer uses a different texture or blending state, the `ui_flush()` function is triggered multiple times per key:

- **Key 1**: Draw Base (Flush) -> Draw Bloom (Flush + State Change) -> Draw Label (Flush)
- **Key 2**: Draw Base (Flush) -> Draw Bloom (Flush + State Change) -> Draw Label (Flush)
- ...
- **Total**: ~280 Draw Calls + ~140 Blending state changes.

For a simple UI, this is extremely inefficient. GPU drivers spend more time managing these state changes than actually pushing pixels, leading to the reported 2ms+ overhead.

## 2. Technical Bottlenecks

### A. Texture Switching

The `UIContext` maintains a single active texture (the font atlas). Functions like `ui_draw_textured_quad` explicitly call `ui_flush()` to swap the font atlas for a specific PNG, then swap it back. Doing this 70 times per frame is highly expensive.

### B. Blending State Changes

`ui_draw_bloom_quad` enables `GL_BLEND` and sets `glBlendFunc(GL_ONE, GL_ONE)` for additive glow, then restores the previous state. State restoration is a synchronous-like operation in the driver.

### C. Fragment Shader Discards

The `ui.frag` shader uses `discard` in almost every branch to handle transparency or SDF edges. While convenient, `discard` can disable some modern GPU optimizations like Early-Z testing.

## 3. Proposed Optimizations

### Optimization 1: Render Batch Reordering (HIGH IMPACT)

Instead of drawing key-by-key, we should draw **layer-by-layer**:

1. **Batch 1**: Draw all 70 Base Textures (1 Draw Call).
2. **Batch 2**: Draw all 70 Bloom Glows (1 Draw Call + 1 State Change).
3. **Batch 3**: Draw all 70 SDF Borders (1 Draw Call).
4. **Batch 4**: Draw all 70 Labels (1 Draw Call).

**Result**: Total draw calls reduced from 280 to **4**.

### Optimization 2: Texture Atlasing (MEDIUM IMPACT)

Combine `kbd_key_base.png`, `kbd_panel_frame.png`, and the font atlas into a single **Mega-texture**. This would allow drawing the base and text in a single batch, further reducing overhead.

### Optimization 3: Vertex-Sided SDF Border

Move some of the SDF logic to the vertex shader or pass it as vertex attributes to avoid complex branching in `ui.frag` for simple shapes.

## 4. Implementation Plan

| Step | Action | Estimated Gain |
| :--- | :--- | :--- |
| 1 | Refactor `app_ui.c` to use a multi-pass drawing logic (Layers). | ~80% reduction in GPU time. |
| 2 | Ensure `ui_begin()` is called at the top level of the overlay. | Stabilizes batching behavior. |
| 3 | Cache `ui_measure_text` results or string lengths. | Minor CPU improvement. |
