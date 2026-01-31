
# Global Exposure Management Analysis
**Date: January 24, 2026**

This analysis details how exposure is defined, managed, and applied in the application, covering both the CPU (management) and GPU (implementation and auto-exposure).

## 1. Definition and Data Structure

Exposure is managed via two distinct, non-overlapping modes: **Manual** and **Automatic**.

### CPU Side (include/postprocess.h, src/postprocess.c)
Parameters are stored in the `PostProcess` structure:

*   **Manual Exposure**:
    *   Struct: `ExposureParams`
    *   Variable: `exposure` (float).
    *   Default: `1.0` (Defined by `DEFAULT_EXPOSURE`).
    *   Control: `postprocess_set_exposure()`.

*   **Automatic Exposure (Eye Adaptation)**:
    *   Struct: `AutoExposureParams`
    *   Variables:
        *   `min_luminance` / `max_luminance`: Clamping range for scene luminance.
        *   `speed_up` / `speed_down`: Adaptation speed (eye opens faster than it closes).
        *   `key_value`: The target middle grey value (Exposure anchor).
    *   Control: `postprocess_set_auto_exposure()`.

### GPU Side (Uniforms & Textures)
*   **Manual**: Transmitted via the `exposure.exposure` uniform.
*   **Automatic**:
    *   `autoExposureTexture`: 1x1 `GL_RGBA32F` texture (Image Load/Store) storing the persistent exposure state.
    *   `lumTexture`: 64x64 `GL_R16F` texture used as an intermediate step for average luminance calculation.

---

## 2. Runpaths and Usage Modes

The mode selection occurs in the render loop `postprocess_end()`.

### Mode A: Automatic Exposure (POSTFX_AUTO_EXPOSURE)
If this flag is enabled, a pre-calculation pass is executed **before** the final render.

1.  **Downsample (Luminance Reduction)**
    *   **File**: `shaders/lum_downsample.frag`
    *   **Action**: Takes the HDR scene (`scene_color_tex`) and reduces it to a 64x64 texture.
    *   **Logic**:
        *   Samples 4x4 blocks.
        *   Calculates luminance: `dot(color, vec3(0.2126, 0.7152, 0.0722))`.
        *   Converts to log: `log2(lum)`.
        *   Averages valid values (ignoring pure blacks/infinite).
    *   **Goal**: Prepare compact data for the Compute Shader.

2.  **Adaptation (Compute Shader)**
    *   **File**: `shaders/lum_adapt.comp`
    *   **Action**: Calculates final exposure with temporal inertia.
    *   **Logic** (Single Thread 1,1,1):
        *   Reads the 64x64 texture and calculates global average Log Luminance.
        *   Converts to Linear Scene Luminance: `exp2(avgLogLum)`.
        *   Clamps luminance between `minLuminance` and `maxLuminance`.
        *   Calculates target: `targetExposure = keyValue / sceneLum`.
        *   **Interpolation**: Reads the *previous* exposure from the 1x1 image and interpolates towards the target using `deltaTime` and `speedUp`/`speedDown`.
        *   Writes the new result to the 1x1 image.

3.  **Final Application**
    *   **File**: `shaders/postprocess/exposure.glsl` (included in `postprocess.frag`).
    *   The `getCombinedExposure()` function detects auto-exposure is active.
    *   It samples the 1x1 texture: `texture(autoExposureTexture, vec2(0.5)).r`.

### Mode B: Manual Exposure (POSTFX_EXPOSURE)
Used if auto-exposure is disabled.

1.  Downsample and Compute Shader passes are **skipped**.
2.  **Final Application**:
    *   `getCombinedExposure()` directly uses the `exposure.exposure` uniform.

### Mode C: No Exposure
If no effect is active, the value `1.0` is used.

---

## 3. Integration in the Pipeline (postprocess.frag)

Exposure is applied at a specific stage in the final fragment shader:

1.  Input (Scene Color).
2.  Chrom. Aberration / Motion Blur.
3.  Depth of Field.
4.  Bloom (Added to color).
5.  **>>> EXPOSURE <<<**: `color *= finalExposure`.
6.  Color Grading & White Balance.
7.  Tone Mapping (HDR to LDR).
8.  Gamma Correction.

**Important Observation**: Exposure is applied as a simple scalar multiplier on the linear HDR signal. This is physically consistent (simulation of camera aperture/ISO) and correctly occurs before Tone Mapping.

## 4. Pre-calculation and Optimizations

*   **Downsample 64x64**: Rather than performing a complex parallel reduction of the full 1080p/4k image, the image is drastically reduced via a very fast fragment shader (`lum_downsample.frag`) which performs an approximate average (4x4 Box Filter with stride). This massively reduces the load for the Compute Shader.
*   **Single Compute Shader**: Adaptation launches only a single WorkGroup (1,1,1) as it processes only 4096 pixels (64x64). It is extremely lightweight.
*   **Persistent Texture**: Using a 1x1 texture as storage allows retaining exposure state from one frame to the next without CPU-GPU roundtrips (no `glGetTexImage` needed for logic, everything stays on GPU).

## Synthetic Summary

| Aspect | Detail |
| :--- | :--- |
| **State Storage** | 1x1 Float Texture (GPU) |
| **Adaptation Logic** | Compute Shader (`lum_adapt.comp`) |
| **Metric** | Log Average Luminance |
| **Input Analysis** | 64x64 Downsampled Texture |
| **Application** | Scalar Multiplication in `postprocess.frag` |
| **Conflict** | Auto-Exposure overrides Manual Exposure |

### Adaptation Pipeline

\dot
digraph AutoExposure {
  rankdir=TD;
  bgcolor="transparent";
  compound=true;
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

  subgraph cluster_gpu {
    label="GPU Pipeline";
    fontname="Helvetica Bold,Arial,sans-serif";
    fontsize=18;
    fontcolor="#7aa2f7";
    style="rounded";
    color="#7aa2f7";
    penwidth=1.5;
    margin=20;

    Scene [label="Scene HDR\n(Full Res)", shape=note, color="#e0af68", fontcolor="#e0af68"];
    Downsample [label="Downsample Shader\n(64x64 Log Avg)", color="#7dcfff", fontcolor="#7dcfff"];
    Compute [label="Adaptation Compute\n(1x1 Thread)", color="#f7768e", fontcolor="#f7768e"];
    History [label="History Texture\n(Previous Frame)", shape=cylinder, color="#bb9af7", fontcolor="#bb9af7"];
    Final [label="Apply Exposure\n(PostProcess)", color="#9ece6a", fontcolor="#9ece6a"];
  }

  Scene -> Downsample [label="Reduce"];
  Downsample -> Compute [label="Current Luma"];
  History -> Compute [label="Read Old", style=dashed];
  Compute -> History [label="Write New (Lerp)"];
  History -> Final [label="Fetch"];
  Scene -> Final [color="#7aa2f7", penwidth=2.5, label="LDR Path"];
}
\enddot
