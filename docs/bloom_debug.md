# Bloom Debugging Guide

This document describes the specialized debug mode for the Bloom post-processing effect.

## Overview

The Bloom effect is constructed through multiple stages (Prefilter, Downsample chain, Upsample chain). The debug mode allows you to visualize these intermediate buffers to verify the luminance thresholding and the blur distribution.

## Controls

- **SHIFT + 'B'**: Cycles through the construction stages.
  - **Final Map**: The final reconstructed bloom texture (Level 0).

  - **Prefilter**: The result of the high-pass filter.

  - **Downsample**: Visualization of a specific level in the downsampling chain.

  - **Upsample**: Visualization of the progressive blending.

- **ALT + 'B'**: Cycles through **Mip Levels** (0 to 4).
  - In **Downsample** mode: Shows the output of each downscaling pass.

  - In **Upsample** mode: Shows the result of the blending at different resolutions.

## Implementation Details

### Rendering Pipeline

When `POSTFX_BLOOM_DEBUG` is active in the `active_effects` bitmask:

1. `fx_bloom_render` executes only up to the requested `debug_step`.

2. The pipeline uses `goto end_bloom` to bypass unnecessary work.

3. The `postprocess.frag` shader detects the debug flag and outputs the sampled `bloomTexture` directly, bypassing other effects (Film Grain, Vignette, etc.) for a clean visualization.

### UI Integration

The current state is displayed in the **Status Overlay** (toggle with F1):

`Bloom Debug: [Stage] | Mip: [N]`
