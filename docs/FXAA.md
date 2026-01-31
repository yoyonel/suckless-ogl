# FXAA 3.11 Implementation & Optimizations

This document details the Fast Approximate Anti-Aliasing (FXAA 3.11) implementation in Suckless-OGL, specifically focusing on the optimizations tailored for the Deferred/Forward PBR pipeline.

## Overview

FXAA is a single-pass, post-processing anti-aliasing technique that reduces jagged edges (aliasing) by analyzing the contrast between pixels (Luma).

**Version**: FXAA 3.11 (PC Quality / Console Performance Hybrid)
**Location**: `shaders/postprocess/fxaa.glsl`

## Key Optimizations

### 1. Luma-in-Alpha (Bandwidth Optimization)
To avoid calculating Luminance (`dot(rgb, vec3(0.299...))`) multiple times per pixel during the edge detection phase, we pre-calculate it during the rendering pass.

- **Storage**: The Alpha channel of the main Scene Color texture.
- **Writer**: `pbr_ibl_billboard.frag`, `pbr_ibl_instanced.frag`, `background.frag`.
- **Reader**: `fxaa.glsl` uses `textureOffset(...).a` to fetch neighbor luminance.
- **Benefit**: Saves ~8 dot products per pixel in the FXAA pass and reduces register pressure.

#### Pipeline Data Flow

\dot
digraph FXAALuma {
  rankdir=LR;
  bgcolor="transparent";
  dpi=96;

  // Suckless-Modern "Ghost" Design Tokens (Upscaled)
  node [
    shape=rect,
    style="rounded",
    fontname="Helvetica,Arial,sans-serif",
    fontsize=16,
    fillcolor="none",
    color="#414868",
    fontcolor="#c0caf5",
    penwidth=2
  ];

  edge [
    color="#565f89",
    fontname="Helvetica,Arial,sans-serif",
    fontsize=18,
    fontcolor="#9aa5ce",
    arrowsize=0.8,
    penwidth=1.2
  ];

  subgraph cluster_pbr {
    label="PBR Pass";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#7dcfff";
    style="rounded";
    color="#7dcfff";
    margin=20;
    FragShader [label="Fragment Shader\n(PBR Objects)", color="#7dcfff", fontcolor="#7dcfff"];
    Calc [label="Luma = sqrt(LinearColor)\n(Gamma Approx)", color="#7dcfff", fontcolor="#7dcfff", style="rounded,dashed"];
    Output [label="Output\nRGBA (RGB + Luma)", color="#7aa2f7", fontcolor="#7aa2f7", penwidth=3];
  }

  subgraph cluster_fxaa {
    label="FXAA Pass";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#bb9af7";
    style="rounded,dashed";
    color="#bb9af7";
    margin=20;
    Input [label="Sampler\n(Screen Texture)", color="#bb9af7", fontcolor="#bb9af7"];
    EdgeDetect [label="Edge Detect\n(Read Alpha)", color="#9ece6a", fontcolor="#9ece6a"];
    Blend [label="Blend\n(Read RGB)", color="#e0af68", fontcolor="#e0af68"];
  }

  FragShader -> Calc;
  Calc -> Output;
  Output -> Input [label="Texture", color="#7aa2f7", penwidth=2];
  Input -> EdgeDetect [label="Fetch Alpha"];
  Input -> Blend [label="Fetch RGB"];
  EdgeDetect -> Blend [label="Mix Factor", style=dotted];
}
\enddot

### 2. sRGB / Gamma Correctness
Edge detection must be performed in Perceptual Space (Gamma) to match human vision. Since the rendering pipeline is linear:
- **Encoding**: PBR shaders calculate Luma using `sqrt(LinearColor)` to approximate the Gamma 2.2 curve before storing it in Alpha.
- **Center Pixel**: If the center pixel was modified by previous post-process effects (e.g., Chromatic Aberration), its Luma is recalculated on-the-fly using `dot(sqrt(rgb), ...)`.

### 3. Dual Mode (Quality vs. Performance)
Defined via preprocessor macro `FXAA_MODE` in `fxaa.glsl`.

| Mode | ID | Description |
| :--- | :--- | :--- |
| **Quality** | `1` | (Default) Uses iterative edge search with variable steps to find the true end of an edge. Solves long edges accurately. |
| **Performance** | `0` | "Console" style. Skips iterative search. Relies solely on local gradients and sub-pixel blending. Faster but less accurate on long edges. |

### 4. Non-Linear Search (Quality Mode)
Instead of stepping 1 pixel at a time, the search loop uses variable step sizes defined in the `quality[]` array (1.0, 1.5, 2.0, 2.0, 8.0). This allows covering a larger search radius (up to ~12 pixels) with only 5 texture fetches.

## Configuration

Settings are controlled via the `PostProcessUBO`:

```glsl
// UBO Layout (std140)
float fxaaQualitySubpix;           // Default: 0.75 (Range 0.0 - 1.0)
float fxaaQualityEdgeThreshold;    // Default: 0.125 (0.166 for Perf, 0.063 for Quality)
float fxaaQualityEdgeThresholdMin; // Default: 0.063 (0.0833 for Perf, 0.0312 for Quality)
```

- **Subpix**: Controls the removal of single-pixel artifacts (fireflies/noise). Higher values = blurrier but less noise.
- **EdgeThreshold**: Minimum contrast required to trigger AA.

## Debugging

Enable `enableFXAADebug` in `app_settings.h` or via Uniform to visualize:
- **Red**: Pixels affected by Edge AA.
- **Blue**: Pixels affected by Subpixel blend.
- **Gray**: Unaffected pixels.
