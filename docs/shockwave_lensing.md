# Shockwave Lensing Effect

## Overview

When an N-body particle crosses the confinement radius, a **billboard-based lensing shockwave** expands from the impact point. The effect renders as a camera-facing quad that samples the scene behind it and applies:

1. **Radial UV displacement** — pixels are pushed outward from the ring center
2. **Chromatic aberration** — R, G, B channels are sampled at different displacement offsets (1.0×, 1.25×, 1.5×)
3. **Additive HDR glow** — ring edge emits body-colored light that drives bloom

## Architecture

```mermaid
graph TD
    A[Scene Geometry] --> B[Scene FBO - RGBA16F]
    B --> C{Any active shockwaves?}
    C -->|No| E[Post-Processing]
    C -->|Yes| D[Grab Pass: glCopyTexSubImage2D]
    D --> F[Billboard Draw]
    F --> G[Fragment Shader: sample grab_tex + distort]
    G --> B
    B --> E
```

### Render Flow

1. **Scene geometry** renders into the HDR FBO (`scene_color_tex`, RGBA16F)
2. **Grab pass**: `glCopyTexSubImage2D` copies `scene_color_tex` → `grab_tex` (lazy-allocated, same format/size)
3. **Billboard draw**: for each active shockwave, a camera-facing quad is rasterized
4. **Fragment shader**: samples `grab_tex` at displaced screen UVs with per-channel chromatic aberration
5. **Post-processing** reads the modified `scene_color_tex` normally

### Key Files

| File | Role |
|------|------|
| `include/shockwave.h` | `ShockwaveRenderer` struct, constants, API |
| `src/shockwave.c` | Grab pass, emit/update/draw logic |
| `shaders/shockwave.vert` | Billboard vertex shader, outputs `vScreenUV` |
| `shaders/shockwave.frag` | Displacement + chromatic aberration + glow |
| `src/scene.c` | Call site with GPU profiler + wireframe support |

## Grab Pass Cost Analysis

The grab pass uses `glCopyTexSubImage2D` — a **GPU-to-GPU** copy (not a CPU readback). Cost is bounded by VRAM bandwidth only:

| Resolution | Format | Size/frame | Bandwidth @ 60 fps | % of 200 GB/s GPU |
|---|---|---|---|---|
| 1920×1080 | RGBA16F (8 B/px) | ~16 MB | ~960 MB/s | **0.5%** |
| 2560×1440 | RGBA16F | ~29 MB | ~1.7 GB/s | **0.9%** |
| 3840×2160 | RGBA16F | ~66 MB | ~3.9 GB/s | **2.0%** |

**Key properties:**

- No CPU stall, no pipeline synchronization
- Cost = 0 when no shockwaves are active (early return before the copy)
- Lazy allocation: `grab_tex` is only created on first use and resized on resolution change
- Alternative: `glCopyImageSubData` (GL 4.3) or `glBlitFramebuffer` could be marginally faster but the difference is imperceptible

## Shader Design

### Vertex Shader (`shockwave.vert`)

The billboard is a unit quad `[-1,1]` scaled to `u_radius` and oriented to face the camera via cross-product basis vectors. Two outputs:

- `vUV` — local quad coordinates `[-1,1]` for the ring profile
- `vScreenUV` — perspective-divided screen coordinates `[0,1]` for scene sampling

### Fragment Shader (`shockwave.frag`)

```text
Ring profile:    Gaussian centered on expanding wavefront
                 ring_radius = 0.3 + 0.6 * progress
                 ring = exp(-8 * (dist - ring_radius)²)

Temporal envelope: sin(π * progress)  →  smooth fade-in/fade-out

Displacement:    factor = 0.06 * intensity * ring * envelope
                 offset = factor * radial_direction

Chromatic aberration:
                 R = sample(grab_tex, screenUV + offset * 1.0)
                 G = sample(grab_tex, screenUV + offset * 1.25)
                 B = sample(grab_tex, screenUV + offset * 1.5)

Additive glow:   color * 3.0 * ring * envelope * intensity * 0.25
```

### Constants

| Constant | Value | Description |
|---|---|---|
| `DISTORT_STRENGTH` | 0.06 | Maximum UV displacement at peak |
| `RING_SHARPNESS` | 8.0 | Gaussian width (higher = thinner ring) |
| `CA_SPREAD_R/G/B` | 1.0 / 1.25 / 1.5 | Per-channel displacement multipliers |
| `HDR_GLOW_SCALE` | 3.0 | Emissive glow intensity |
| `GLOW_MIX` | 0.25 | Glow contribution vs pure distortion |

## Emission Filtering

Not every boundary crossing produces a visible shockwave. Three filters apply:

1. **Velocity threshold** (`SHOCKWAVE_MIN_VELOCITY = 0.05`): only the outward radial velocity component is measured (`v_out = dot(velocity, radial_hat)`). Tangential crossings produce no effect
2. **Peak tracking**: per body, only the peak velocity during a frame is recorded (avoids duplicate events from sub-stepping)
3. **Capacity limit** (`SHOCKWAVE_MAX_ACTIVE = 16`): when full, the oldest event is evicted

## Time Reversal Support

The simulation supports backward time via `time_scale < 0`. Shockwaves use `fabsf(sim_time - start_time)` for age computation, ensuring:

- Ring expansion is always outward regardless of time direction
- Cleanup removes events after `SHOCKWAVE_DURATION` absolute seconds
- No accumulation of stale events in reverse mode

## Profiling & Observability

The shockwave draw is instrumented at three levels:

| Tool | Mechanism | Visibility |
|---|---|---|
| **GPU Profiler (F3)** | `GPU_STAGE_PROFILER("Shockwave VFX")` | In-app overlay, timer query |
| **Tracy** | `TracyCZoneN` + `tracy_gpu_zone_begin/end` | Tracy profiler (build with `TRACY_ENABLE`) |
| **RenderDoc** | `gl_debug_push_group("Shockwave_VFX")` | RenderDoc capture groups |

The profiler stage includes both the grab pass and the billboard draw calls, giving the total GPU cost of the effect.

## Wireframe Debug

When wireframe mode is active (W key), shockwave billboards render as line quads via `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)`. This shows:

- Billboard quad geometry and orientation
- Ring expansion radius over time
- Number of active shockwaves
