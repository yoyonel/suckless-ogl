# Shader Cross-GPU Compatibility Guidelines

> **Case Study**: See [gpu-rendering-synchronization.md](./gpu-rendering-synchronization.md) for a real-world example of Intel vs NVIDIA fixes

## Overview

Best practices for writing OpenGL shaders that produce consistent results across GPU vendors (Intel, NVIDIA, AMD).

## Key Principles

### 1. Avoid Relying on Derivative Precision

**Problem**: `dFdx()` and `dFdy()` have vendor-specific implementations.

**Recommendation**: Use derivatives only for debugging/visualization, not critical rendering logic.

**Alternative**: Pre-compute values in geometry shaders or use distance-based heuristics.

### 2. Pre-Calculate and Store Values

**Problem**: Recalculating the same value multiple times can accumulate precision errors differently.

**Solution**: Calculate once, store in unused texture channels (e.g., alpha).

**Example**:
\code{.glsl}
// Fragment shader (once)
float luma_val = dot(sqrt(color_val), vec3(0.2126, 0.7152, 0.0722));
FragColor_out = vec4(color_val, luma_val);  // Store in alpha

// Post-processing (reuse)
float stored_luma = texture(tex_sampler, uv_coords).a;  // Consistent across vendors
\endcode

### 3. Use Explicit Precision Qualifiers (Mobile/WebGL)

\code{.glsl}
#ifdef GL_ES
precision highp float;
precision highp int;
#endif
\endcode

**Note**: Desktop OpenGL ignores these, but they're critical for mobile/WebGL.

### 4. Clamp Intermediate Values

Prevent overflow/underflow in calculations:

\code{.glsl}
// Bad
float val_bad = pow(someInput, 0.1);

// Good
float val_good = pow(clamp(someInput, 0.0, 10.0), 0.1);
\endcode

### 5. Avoid Fast Math Assumptions

Use explicit parentheses to control floating-point operation order:

\code{.glsl}
// Order-dependent
float result_val = ((a * b) + (c * d)) + (e * f);
\endcode

## Common Pitfalls

### Pitfall 1: Texture LOD Calculation

\code{.glsl}
// Implicit LOD may differ
vec3 color_res = texture(envMap_tex, direction).rgb;

// Explicit LOD is consistent
vec3 color_res_fixed = textureLod(envMap_tex, direction, roughness * 4.0).rgb;
\endcode

### Pitfall 2: Small Exponents in pow()

\code{.glsl}
// Very sensitive to input differences
float bad_pow = pow(variation, 0.1);   // 10th root

// More stable
float better_sqrt = sqrt(variation);     // Square root
\endcode

### Pitfall 3: Derivatives in Divergent Branches

\code{.glsl}
// BAD: Undefined behavior
if (someCondition) {
    float dx_val_bad = dFdx(value_val);
}

// GOOD: Compute before branching
float dx_precomputed = dFdx(value_val);
if (someCondition) {
    // Use dx_precomputed
}
\endcode

## Testing Workflow

1. **Visual Comparison**: Test on 2+ GPU vendors
2. **Pixel Diff**: Use image comparison tools (see [Visual Testing Artifacts](./visual_testing_artifacts.md))
3. **Frame Capture**: Compare with RenderDoc/ApiTrace
4. **Driver Versions**: Test with different driver releases

### Tools

\code{.bash}
# Visual diff
compare intel.png nvidia.png diff.png

# ApiTrace
apitrace trace ./app
apitrace replay app.trace
\endcode

## When to Use Derivatives

✅ **Safe uses**:
- Debug visualization (where exact parity is not required)
- Non-critical effects (optional grain, etc.)
- Explicitly documented vendor-specific behavior

❌ **Avoid for**:
- Core rendering logic where cross-GPU "Difference Maps" must be clean
- Anti-aliasing (prefer FXAA or Analytic smoothing)
- Material property adjustments (use `MIN_ROUGHNESS` constants)

## Implementation Example: Analytic Edge Smoothing

Instead of `fwidth(h)`:

\code{.glsl}
// h = discriminant (0 at edge)
// analyticFwidth = footprint of pixel in h-space
float edgeFactor_val = clamp(h / analyticFwidth, 0.0, 1.0);
\endcode

See [pbr_ibl_billboard.frag](../shaders/pbr_ibl_billboard.frag) for a production example.

More details on why testing artifacts appear in CI can be found in [Visual Testing & Regression Artifacts](./visual_testing_artifacts.md).

## References

- [OpenGL Derivative Functions](https://www.khronos.org/opengl/wiki/Derivative)
- [GLSL Precision Qualifiers](https://www.khronos.org/opengl/wiki/Type_Qualifier_(GLSL)#Precision_qualifiers)
- [Floating Point Determinism](https://randomascii.wordpress.com/2013/07/16/floating-point-determinism/)
