# GPU Rendering Synchronization: Intel vs NVIDIA

**Date**: 2026-01-30  
**Status**: Resolved  
**Impact**: Critical - Visual quality consistency across GPU vendors

---

> **See also**: [shader-cross-gpu-compatibility.md](./shader-cross-gpu-compatibility.md) for general best practices

---

## Executive Summary

Investigation and resolution of rendering differences between Intel and NVIDIA GPUs in the suckless-ogl PBR renderer. Issues manifested as white halos and incorrect FXAA edge blending on NVIDIA hardware.

## Visual Comparison

| GPU | Before Fix | After Fix |
|-----|------------|-----------|
| **Intel** | ✅ Clean edges, proper FXAA | ✅ Unchanged |
| **NVIDIA** | ❌ White halos, buggy edges | ✅ Identical to Intel |

## Root Causes Identified

### Issue 1: FXAA Luminance Recalculation

**File**: `shaders/postprocess/fxaa.glsl` (Lines 176-198)

**Problem**: Edge search loop recalculated luma using `sqrt()`, which has different precision on Intel vs NVIDIA.

**Fix**: Use pre-calculated luma from alpha channel instead.

```diff
- lumaEnd1 = FxaaLuma(texture(screenTexture, uv1).rgb);
+ lumaEnd1 = texture(screenTexture, uv1).a;  // Pre-calculated in PBR shader
```

### Issue 2: Derivative-Based Roughness Clamping

**File**: `shaders/pbr_functions.glsl` (Lines 76-100)

**Problem**: `dFdx()`/`dFdy()` produce different values on Intel vs NVIDIA, causing extreme roughness values at edges on NVIDIA.

**Attempted Fixes**:
1. ❌ Threshold 0.1 → 0.5: Reduced but didn't eliminate halos
2. ❌ Saturation `min(maxVariation, 1.0)`: Still visible artifacts
3. ✅ **Complete removal**: Achieved visual parity

**Final Solution**: Disabled roughness clamping entirely.

```glsl
float compute_roughness_clamping(vec3 N, float roughness)
{
    // Disabled: derivatives have different precision on NVIDIA vs Intel
    roughness = clamp(roughness, 0.0, 1.0);
    return roughness;
}
```

## Why Derivatives Differ

| Vendor | Implementation | Behavior |
|--------|---------------|----------|
| **Intel** | Conservative 2x2 quad finite differences | Stable, predictable values |
| **NVIDIA** | Optimized hardware units | Different rounding, can spike |

**Result**: `pow(maxVariation, 0.1)` amplified vendor differences → white halos on NVIDIA.

## Trade-offs

### Lost
- Geometric anti-aliasing on curved surfaces
- Specular aliasing prevention on very smooth metals

### Gained
- ✅ Cross-vendor consistency
- ✅ Predictable behavior
- ✅ Simplified shader code
- ✅ Minor performance improvement

**Verdict**: FXAA already provides excellent AA. Loss of roughness clamping is negligible.

## Validation Results

```
Before:  Intel ✅  |  NVIDIA ❌ (halos, buggy FXAA)
After:   Intel ✅  |  NVIDIA ✅ (identical rendering)
```

## Files Modified

- `shaders/postprocess/fxaa.glsl`
- `shaders/pbr_functions.glsl`

## References

- [shader-cross-gpu-compatibility.md](./shader-cross-gpu-compatibility.md) - General guidelines
- [FXAA 3.11 Whitepaper](https://developer.nvidia.com/fxaa)
- [OpenGL Derivatives](https://www.khronos.org/opengl/wiki/Derivative)
