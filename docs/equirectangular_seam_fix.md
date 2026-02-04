# Equirectangular IBL Seam Resolution

This document describes the resolution of the vertical seam artifact observed in the IBL (Image Based Lighting) environment maps when using equirectangular projection.

## Problem Description

A faint, perfectly straight vertical line was visible in the environment reflection (Specular IBL) and irradiance (Diffuse IBL), most noticeably in "Irradiance (Diff)" debug mode. This line appeared at the UV wrap-around point (where the azimuthal angle $\phi$ jumps from $+\pi$ to $-\pi$).

The issue was caused by two cooperative factors:

1.  **Horizontal Wrap Mode**: The generated IBL textures (Irradiance Map and Prefiltered Specular Map) were initialized with `GL_CLAMP_TO_EDGE` for the `GL_TEXTURE_WRAP_S` parameter. When sampling near the $u=1.0$ or $u=0.0$ boundary, linear filtering would "clamp" to the edge pixel instead of blending with the other side of the equirectangular map.
2.  **GPU Derivative Discontinuity**: When using the standard `texture()` function, the GPU calculates automatic MIP levels based on screen-space derivatives ($dFdx$ / $dFdy$). At the UV wrap-around point, the coordinate jumps from $1.0$ to $0.0$ (or vice versa) in a single pixel, creating an enormous gradient. This triggers the GPU to select the coarsest MIP level (highly blurred) for that specific pixel column, creating a "seam".

## Resolution

The fix involves two changes:

### 1. Correcting Wrap Modes (C Code)

In `src/pbr.c`, the horizontal wrap mode for all equirectangular-mapped IBL textures has been changed from `GL_CLAMP_TO_EDGE` to `GL_REPEAT`.

```c
// src/pbr.c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

This ensures that linear filtering correctly wraps around the "seam" of the sphere.

### 2. Bypassing Automatic MIP Selection (Shader)

In `shaders/pbr_functions.glsl`, the sampling of the irradiance map (which is already pre-convolved and does not require complex MIP selection for diffusion) has been updated to use `textureLod` with an explicit level of `0.0`.

```glsl
// shaders/pbr_functions.glsl
vec3 irradiance = textureLod(irradianceMap, dirToUV(N), 0.0).rgb;
```

For the specular prefiltered map, which *does* use MIP levels for roughness, the wrap mode fix in C is usually sufficient to hide the seam, as the derivatives only trigger the wrong MIP at the exact jump point, and `GL_REPEAT` allows `texture()` to handle wrap-around derivatives more gracefully on some hardware (though `textureLod` remains the safest choice for irradiance).

## Verification

The fix can be verified by:
1. Enabling "Billboard" mode.
2. Switching to PBR Debug Mode 6 ("Irradiance (Diff)").
3. Rotating the camera to view the $X=0$ alignment where the seam previously appeared.
4. Shading should now be perfectly continuous across the entire sphere.

> [!NOTE]
> As IBL maps are generated at application startup, a full restart or environment reload is required to see the effects of the wrap mode change.
