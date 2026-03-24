# Cinematic Rendering & Nordic Noir

This document describes the high-fidelity cinematic rendering system implemented in March 2026, specifically focusing on the **Nordic Noir** aesthetic.

## 🎨 The Nordic Noir Aesthetic

The "Nordic Noir" look (Key **9**) is a professional-grade color grade and atmospheric setup designed for high-contrast, moody overnight or wintry scenes. It is characterized by:

* **Elevated Shadows**: Using the `cg_lift` parameter to create "milky" blacks that preserve texture.
* **Cool Palette**: A white balance push towards **4050K** for a deep teal/cyan atmosphere.
* **Physical Atmosphere**: A dense, height-aware fog system and organic 35mm film grain.

## 🌫️ Atmospheric Fog System

The system uses a depth-based exponential fog with several high-end features:

### Height-Based Falloff

Unlike simple linear fog, the density decays exponentially with vertical height (`y` coordinate). This allows for realistic ground-level haze while keeping the sky clear.

$$ Fog(h) = d \cdot e^{-falloff \cdot h} $$

### Spectral Shift

To simulate real-world Rayleigh scattering, the fog color shifts towards a deep teal (`0.10, 0.16, 0.22`) as it gets denser in the distance. This provides much more depth than a single-color fog.

### Skybox Masking

The fog is masked from the skybox using depth/stencil testing. This prevents the "washed out" look common in simple implementations and preserves the deep contrast of the background imagery.

### Debug Mode (Shift + F7)

A dedicated diagnostic view allows developers to isolate only the fog component (rendered against a black background). This is invaluable for balancing density and height falloff.

## 🎞️ Photographic Film Grain

The grain system emulates 35mm film stock through a perceptual noise model:

* **Temporal Jitter**: Instead of "scrolling" noise, the system uses a high-frequency hash of time to regenerate a unique noise grain every frame. This prevents directional streaks and creates a natural "boiling" effect.
* **Luminance Weighting**: Grain intensity is dynamically scaled based on the scene luminance. It is densest in the shadows and midtones, and barely visible in clear highlights, mimicking real silver halide behavior.

## 📊 Monitoring & Control

The rendering parameters are exposed in the **F1 Detailed Overlay**:

* **WB Temp**: Real-time color temperature in Kelvin.
* **Shadow Lift**: Current elevation of the black point.
* **Fog Metrics**: Density and start distance.

All parameters can be calibrated via the `PostProcessPreset` structures in `include/postprocess_presets.h`.
