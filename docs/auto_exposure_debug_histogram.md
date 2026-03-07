# Auto Exposure Debug Histogram

This document details the implementation and testing of the luminance histogram displayed in the "Auto Exposure Debug" view.

## Overview

The Auto Exposure Debug view provides a real-time visualization of the scene's luminance distribution. This is essential for tuning auto-exposure parameters such as `min_luminance`, `max_luminance`, and `key_value`.

### Key Features
- **Real-time Histogram**: Displays the distribution of luminance across 256 buckets.
- **PBO Integration**: Uses Pixel Buffer Objects (PBOs) for asynchronous readback from VRAM to RAM, minimizing CPU stalls.
- **Continuity Caching**: Implements a cache to ensure the histogram display remains stable even when the GPU is slow to update the data.
- **Independence from AE State**: The histogram calculation and rendering are automatically triggered when the debug view is active, even if the actual Auto Exposure effect is disabled.

## Technical Implementation

### 1. Luminance Downsampling
Before the histogram can be calculated, the scene is downsampled to a 64x64 texture (`LUM_HISTOGRAM_MAP_SIZE`) where each texel represents the log-luminance of a block of pixels.
- **Shader**: `shaders/lum_downsample.frag`

### 2. Asynchronous Readback (PBO)
Instead of a direct `glGetTexImage` (which is a blocking operation), the readback is requested at the end of the post-processing pass and collected in subsequent frames.
- **Double Buffering**: Two PBOs (`histogram_pbo[2]`) are used to alternate between request and readback frames.
- **Sync Objects**: `glFenceSync` is used to track when the GPU has finished writing to the PBO.

### 3. Caching & Continuity Logic
To avoid the UI flickering when a frame's readback isn't ready, the system keeps the last successfully calculated histogram data in a cache.
- **Cache Fields** (`PostProcess` struct):
  - `last_buckets[256]`
  - `last_min_lum`
  - `last_max_lum`
  - `last_histogram_updated` (flag)

When `postprocess_compute_luminance_histogram` is called:
1. It checks if the current PBO sync object is signaled.
2. If signaled:
   - It maps the PBO and zeros out the `last_buckets` cache.
   - It re-calculates the histogram from the new data.
   - It unmaps the PBO and updates the cache timestamp/flag.
3. It always returns the contents of the cache if `last_histogram_updated` is true, providing a seamless visual experience.

## Testing Strategy

A dedicated test suite `tests/test_auto_exposure_debug.c` ensures the robustness of this system.

### Test Cases
- **`test_histogram_caching_and_continuity`**: Verifies that the cache correctly provides data when the sync object is not yet signaled, and updates correctly when it is.
- **`test_histogram_auto_trigger_when_ae_off`**: Confirms that enabling `POSTFX_EXPOSURE_DEBUG` automatically triggers the `fx_auto_exposure_render` pass inside `postprocess_end`, ensuring live data is available even if Auto Exposure is OFF.
- **`test_histogram_cache_no_accumulation`**: Ensures that the `last_buckets` cache is properly cleared before being filled with new data, preventing values from "climbing" or stagnating over time.

## Visual Verification

When active, the histogram shows a dynamic bar chart. If the scene changes (e.g., looking at a light source), the histogram should shift instantly to the right. The caching mechanism ensures this movement is smooth without dropped frames or flickering to zero.
