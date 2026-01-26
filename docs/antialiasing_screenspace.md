# Screen-Space Anti-Aliasing: Study and Implementation

> [!NOTE]
> This document provides a comprehensive study of screen-space anti-aliasing methods and details the FXAA implementation in this project.

## Table of Contents
1. [Introduction to Aliasing](#introduction-to-aliasing)
2. [Traditional Multi-Sampling Methods](#traditional-multi-sampling-methods)
3. [Screen-Space Anti-Aliasing Methods](#screen-space-anti-aliasing-methods)
4. [Method Comparison](#method-comparison)
5. [FXAA Implementation in This Project](#fxaa-implementation-in-this-project)
6. [Performance Considerations](#performance-considerations)
7. [References](#references)

---

## Introduction to Aliasing

**Aliasing** occurs when high-frequency details in a scene (sharp edges, thin lines, specular highlights) cannot be properly represented at the output resolution, resulting in:
- **Jagged edges** (stair-stepping on diagonal lines)
- **Temporal flickering** (shimmering on moving geometry)
- **Moiré patterns** (interference patterns on fine textures)

Anti-aliasing (AA) techniques aim to reduce these artifacts by increasing sampling density or reconstructing higher-quality pixels through filtering.

---

## Traditional Multi-Sampling Methods

### SSAA (Super-Sample Anti-Aliasing)
- **Method**: Render the entire scene at a higher resolution (e.g., 2x, 4x), then downsample to the target resolution.
- **Quality**: Excellent—removes aliasing from geometry, textures, and shading.
- **Cost**: Extremely expensive (4x SSAA = 4x pixel shading cost, 4x memory bandwidth).
- **Use Case**: Offline rendering, high-end screenshots.

### MSAA (Multi-Sample Anti-Aliasing)
- **Method**: Render geometry at multiple samples per pixel but run the fragment shader only once per pixel. Subsamples are used only for edge coverage.
- **Quality**: Good for geometric edges; no improvement for shader aliasing (specular, texture, alpha-tested geometry).
- **Cost**: Moderate (2x-8x samples = 2x-8x framebuffer memory + resolve cost).
- **Limitations**:
  - Ineffective on deferred rendering pipelines (G-buffer explosion).
  - Doesn't address shader aliasing (high-frequency normal maps, specular).
  - Wasted on modern complex shaders.

---

## Screen-Space Anti-Aliasing Methods

Screen-space techniques operate as **post-process filters** on the final rendered image, analyzing patterns to detect and smooth edges without requiring multi-sampling or higher resolution rendering.

### FXAA (Fast Approximate Anti-Aliasing)
**Developed by**: NVIDIA (Timothy Lottes), 2009

**Algorithm**:
1. **Luminance Calculation**: Convert RGB to perceptual luminance for edge detection.
2. **Local Contrast Detection**: Analyze 3x3 pixel neighborhood to identify high-contrast edges.
3. **Edge Orientation**: Determine if the edge is horizontal or vertical.
4. **Subpixel Shift**: Blend neighboring pixels along the edge direction to smooth jagged transitions.
5. **End-of-Edge Detection**: Walk along the edge to find its endpoints, preventing over-blurring.

**Strengths**:
- **Fast**: Single-pass, minimal memory bandwidth (1 read + 1 write per pixel).
- **Universal**: Works on all aliasing types (geometry, shader, alpha-tested).
- **Simple Integration**: No multi-pass setup, no temporal buffer, no motion vectors.
- **Tunable**: Quality scales with configurable thresholds.

**Weaknesses**:
- **Over-blurring**: Can blur textures and intentional high-frequency details.
- **No temporal stability**: Each frame is processed independently.
- **Luminance-based**: May miss color-only edges (rare in practice).

**Quality Presets**:
| Preset | Subpixel | EdgeThreshold | EdgeThresholdMin | Use Case |
|--------|----------|---------------|------------------|----------|
| **Low** | 0.25 | 0.166 | 0.0833 | Performance-critical, mobile |
| **Medium** | 0.75 | 0.125 | 0.0625 | Balanced (default) |
| **High** | 0.75 | 0.063 | 0.0312 | Quality-focused desktops |
| **Ultra** | 1.0 | 0.063 | 0.0312 | Maximum smoothing |

---

### SMAA (Subpixel Morphological Anti-Aliasing)
**Developed by**: Jorge Jimenez et al., 2011

**Algorithm**:
1. **Edge Detection**: Similar to FXAA, but uses depth/stencil for better accuracy.
2. **Pattern Matching**: Pre-computed lookup textures classify edge shapes (L-shape, U-shape, Z-shape).
3. **Blending**: Reconstruct pixels using weighted blending based on pattern type.

**Strengths**:
- **Better edge reconstruction** than FXAA (uses morphological patterns).
- **Less blurring** on textures.
- **Temporal variant (SMAA T2x)** improves stability.

**Weaknesses**:
- **Multi-pass**: 3 passes (edge detection, blending weight calculation, neighborhood blending).
- **Requires lookup textures** (~1 MB additional memory).
- **More complex integration** than FXAA.

---

### TAA (Temporal Anti-Aliasing)
**Algorithm**:
1. **Jittering**: Offset the projection matrix slightly each frame (subpixel jitter).
2. **History Reprojection**: Accumulate previous frames using motion vectors.
3. **Temporal Blending**: Blend current frame with reprojected history (typically 90% history, 10% current).

**Strengths**:
- **Excellent quality**: Effectively removes all aliasing, including shader and temporal flickering.
- **Supersampling effect**: Accumulates subpixel detail over time.
- **Standard in modern games** (Unreal Engine, Unity HDRP).

**Weaknesses**:
- **Ghosting**: Fast-moving objects leave trails if motion vectors are inaccurate.
- **Requires motion vectors**: Every object must output velocity.
- **Disocclusion handling**: Complex logic needed for newly visible areas.
- **Temporal lag**: Input latency can feel "mushy" in fast-paced games.

---

### Other Methods
- **MLAA (Morphological AA)**: Early CPU-based morphological AA (Intel, 2009). Superseded by SMAA.
- **DLAA (Deep Learning AA)**: NVIDIA's AI-based AA (2021). Requires Tensor cores.
- **CMAA (Conservative Morphological AA)**: Intel's GPU-optimized MLAA variant.

---

## Method Comparison

| Method | Quality | Performance | Integration Effort | Temporal Stability | Shader Aliasing |
|--------|---------|-------------|-------------------|-------------------|-----------------|
| **SSAA** | ★★★★★ | ★☆☆☆☆ | Easy | Excellent | Excellent |
| **MSAA** | ★★★☆☆ | ★★☆☆☆ | Medium | Excellent | Poor |
| **FXAA** | ★★★☆☆ | ★★★★★ | Very Easy | Fair | Good |
| **SMAA** | ★★★★☆ | ★★★★☆ | Medium | Good (T2x) | Good |
| **TAA** | ★★★★★ | ★★★☆☆ | Hard | Excellent | Excellent |

**Recommendation**:
- **For this project (PBR + deferred shading + billboards)**: **FXAA** is ideal due to:
  - Alpha-to-Coverage billboards benefit from post-process smoothing.
  - Deferred pipeline makes MSAA impractical.
  - TAA requires motion vectors (additional complexity).
  - FXAA provides good quality with minimal overhead.

---

## FXAA Implementation in This Project

### Integration Overview

FXAA is implemented as the **final pass** in the post-processing pipeline, operating on the tone-mapped, gamma-corrected image just before UI rendering.

**Pipeline Position**:
```
Scene Rendering (PBR + IBL)
  ↓
Post-Process Effects (Bloom, DOF, Motion Blur, etc.)
  ↓
Tone Mapping + Gamma Correction
  ↓
**FXAA** ← Final AA pass
  ↓
UI Rendering
  ↓
Present to Screen
```

---

### File Structure

| File | Purpose |
|------|---------|
| [`shaders/postprocess/fxaa.glsl`](file:///home/latty/Prog/__PERSO__/suckless-ogl/shaders/postprocess/fxaa.glsl) | FXAA shader implementation (included in main shader) |
| [`shaders/postprocess.frag`](file:///home/latty/Prog/__PERSO__/suckless-ogl/shaders/postprocess.frag) | Main post-process shader calling `applyFXAA()` |
| [`include/postprocess.h`](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/postprocess.h) | `FxaaParams` struct definition |
| [`src/postprocess.c`](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/postprocess.c) | FXAA parameter initialization and UBO upload |

---

### Shader Implementation Details

#### Core Function: `applyFXAA()`
Location: [`shaders/postprocess/fxaa.glsl`](file:///home/latty/Prog/__PERSO__/suckless-ogl/shaders/postprocess/fxaa.glsl)

**Parameters** (from UBO):
```glsl
struct FxaaParams {
    float subpix;            // Subpixel aliasing removal (0.0-1.0)
    float edgeThreshold;     // Edge detection threshold (0.063-0.333)
    float edgeThresholdMin;  // Minimum threshold for dark areas
    float _pad;              // Padding for std140 alignment
};
```

**Key Steps**:
1. **Luminance Calculation**:
   ```glsl
   vec3 rgbNW = textureOffset(colorTexture, texCoord, ivec2(-1, 1)).rgb;
   // ... (sample 4 neighbors)
   float lumaNW = dot(rgbNW, luma);
   // ...
   ```

2. **Contrast Detection**:
   ```glsl
   float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
   float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
   float contrast = lumaMax - lumaMin;

   if (contrast < max(params.edgeThresholdMin, lumaMax * params.edgeThreshold)) {
       return rgbM; // Skip low-contrast areas
   }
   ```

3. **Edge Direction**:
   ```glsl
   float edgeHorz = abs((lumaNW + lumaNE) - 2.0 * lumaM) +
                    abs((lumaSW + lumaSE) - 2.0 * lumaM);
   float edgeVert = abs((lumaNW + lumaSW) - 2.0 * lumaM) +
                    abs((lumaNE + lumaSE) - 2.0 * lumaM);
   bool isHorizontal = (edgeHorz >= edgeVert);
   ```

4. **Subpixel Blending**:
   ```glsl
   float subpixelBlend = smoothstep(0.0, 1.0,
       abs(lumaM - lumaAvg) / contrast);
   // Blend with neighbors based on subpixel parameter
   ```

5. **Edge-Aligned Sampling**:
   - Walk along the perpendicular direction to the edge.
   - Sample pixels until reaching the end of the edge or contrast drops.
   - Compute final blend weight to smooth the transition.

---

### C Implementation

#### Parameter Struct
[`include/postprocess.h`](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/postprocess.h):
```c
typedef struct {
    float subpix;            // 0.0-1.0, subpixel AA strength
    float edgeThreshold;     // 0.063-0.333, main edge detection
    float edgeThresholdMin;  // 0.0-0.1, dark area sensitivity
    float _pad;              // Alignment for std140
} FxaaParams;
```

#### Initialization
[`src/postprocess.c`](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/postprocess.c):
```c
// Medium Quality preset (balanced)
state->fxaa_params = (FxaaParams){
    .subpix = 0.75f,
    .edgeThreshold = 0.125f,
    .edgeThresholdMin = 0.0625f,
    ._pad = 0.0f
};
```

**Alternative presets**:
```c
// Ultra Quality
.subpix = 1.0f,
.edgeThreshold = 0.063f,
.edgeThresholdMin = 0.0312f

// Low (Performance)
.subpix = 0.25f,
.edgeThreshold = 0.166f,
.edgeThresholdMin = 0.0833f
```

#### UBO Upload
The FXAA parameters are uploaded to the GPU via the **Unified Buffer Object (UBO)** in the `postprocess_end()` function:
```c
ubo_data.fxaa = state->fxaa_params;
glBindBuffer(GL_UNIFORM_BUFFER, state->ubo);
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PostProcessUBO), &ubo_data);
```

---

### Parameter Tuning Guide

#### `subpix` (Subpixel Aliasing)
- **Range**: `0.0` (off) to `1.0` (maximum)
- **Effect**: Controls how aggressively FXAA smooths subpixel details (single-pixel lines, thin edges).
- **Tuning**:
  - **0.0-0.25**: Minimal smoothing, preserves texture sharpness.
  - **0.5-0.75**: Balanced (recommended for most scenes).
  - **1.0**: Maximum smoothing, may blur fine details.

#### `edgeThreshold`
- **Range**: `0.063` (sensitive) to `0.333` (conservative)
- **Effect**: Main threshold for edge detection. Lower values detect more edges.
- **Tuning**:
  - **0.063**: Detects very subtle edges (high quality, may over-process).
  - **0.125**: Balanced (default).
  - **0.166-0.333**: Performance mode, only processes obvious edges.

#### `edgeThresholdMin`
- **Range**: `0.0` to `0.1`
- **Effect**: Absolute minimum contrast required to trigger FXAA, used in dark areas.
- **Tuning**:
  - **0.0312**: Aggressive (processes even low-contrast dark areas).
  - **0.0625**: Balanced.
  - **0.0833**: Conservative (skips subtle dark edges).

**Visual Tuning Workflow**:
1. Start with **Medium** preset.
2. If **aliasing remains visible** on thin lines → Lower `edgeThreshold` to `0.063`.
3. If **textures look blurry** → Lower `subpix` to `0.5` or `0.25`.
4. If **dark areas flicker** → Lower `edgeThresholdMin` to `0.0312`.
5. If **performance is critical** → Use **Low** preset.

---

## Performance Considerations

### Cost Analysis
- **GPU Time**: ~0.3-0.5 ms @ 1080p (on mid-range GPU).
- **Memory Bandwidth**: Single texture read per pixel (~8 MB @ 1080p for RGBA8).
- **ALU Cost**: Minimal (simple arithmetic, no expensive functions).

### FXAA vs. MSAA Performance
Assuming 1920x1080 resolution:

| Method | Framebuffer Size | Bandwidth | Shader Cost | Total Impact |
|--------|------------------|-----------|-------------|--------------|
| **No AA** | 8 MB | 1x | 1x | Baseline |
| **MSAA 4x** | 32 MB | 4x | 1x | +15-30% frame time |
| **FXAA** | 8 MB | 1.2x | 1.05x | +2-5% frame time |

**Key Insight**: FXAA is **5-10x cheaper** than MSAA 4x while providing comparable or better results for shader aliasing.

### Optimization Tips
1. **Run FXAA on tone-mapped output**: Ensures luminance calculations are perceptually correct.
2. **Skip UI layers**: Apply FXAA before UI rendering to avoid blurring text.
3. **Use `textureOffset()`**: Direct offset fetches are faster than manual UV arithmetic.
4. **Early exit on low contrast**: The threshold check saves ~70% of pixels from full processing.

---

## Interaction with Other AA Methods

### FXAA + Alpha-to-Coverage (A2C)
**This project uses both**:
- **A2C**: Provides geometric anti-aliasing for billboard edges (requires MSAA context).
- **FXAA**: Smooths remaining aliasing in shading, textures, and post-A2C artifacts.

**Synergy**:
- A2C handles the "hard" edge geometry problem (circle-to-rectangle billboards).
- FXAA polishes the final image, removing high-frequency noise from PBR specular, normal maps, and resolve artifacts.

**Note**: When `DEFAULT_SAMPLES = 1` (MSAA disabled), A2C is non-functional, but FXAA remains fully effective.

---

### FXAA + TAA (Future Work)
If temporal anti-aliasing is added in the future:
- **TAA First**: Accumulate jittered frames to remove aliasing.
- **FXAA Last**: Apply as a "safety net" to clean up TAA disocclusion artifacts and residual noise.

---

## Known Limitations

1. **Over-Blurring Fine Textures**:
   - FXAA cannot distinguish between aliasing and intentional high-frequency details (e.g., text, sharp UI elements).
   - **Mitigation**: Apply FXAA before UI rendering, or mask high-detail regions.

2. **Static-Only**:
   - FXAA processes each frame independently, providing no temporal stability.
   - **Mitigation**: Combine with TAA or camera motion smoothing.

3. **Color-Edge Blindness**:
   - FXAA relies on luminance, so pure-color edges (e.g., red-to-blue transition with equal luminance) may be missed.
   - **Mitigation**: This is extremely rare in real-world rendering due to lighting variations.

---

## References

### Papers and Specifications
1. **FXAA Whitepaper**: Timothy Lottes, NVIDIA, 2009
   [https://developer.nvidia.com/fxaa](https://developer.nvidia.com/fxaa)

2. **SMAA**: Jorge Jimenez et al., "SMAA: Enhanced Subpixel Morphological Antialiasing", 2011
   [http://www.iryoku.com/smaa/](http://www.iryoku.com/smaa/)

3. **TAA in Unreal Engine**: Brian Karis, "High-Quality Temporal Supersampling", SIGGRAPH 2014
   [https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf](https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf)

### Code References
- **FXAA 3.11 Implementation**: [NVIDIA GameWorks](https://github.com/NVIDIAGameWorks/GraphicsSDK)
- **This Project**: See files listed in [File Structure](#file-structure) section above.

---

## Conclusion

**FXAA** was chosen for this project because:
1. **Low cost**: ~2-5% performance impact vs. 15-30% for MSAA 4x.
2. **Universal coverage**: Works on all aliasing sources (geometry, shaders, billboards).
3. **Simple integration**: Single-pass post-process, no pipeline changes.
4. **Complements A2C**: Polishes billboard edges after multi-sample resolve.

For **future improvements**, consider:
- **TAA**: For best-quality temporal stability (requires motion vectors).
- **SMAA**: If more precise edge reconstruction is needed without temporal artifacts.

The current **Medium-to-Ultra** FXAA preset provides excellent quality for real-time PBR rendering at negligible cost.
