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
```glsl
// Fragment shader (once)
float luma = dot(sqrt(color), vec3(0.299, 0.587, 0.114));
FragColor = vec4(color, luma);  // Store in alpha

// Post-processing (reuse)
float luma = texture(tex, uv).a;  // Consistent across vendors
```

### 3. Use Explicit Precision Qualifiers (Mobile/WebGL)

```glsl
#ifdef GL_ES
precision highp float;
precision highp int;
#endif
```

**Note**: Desktop OpenGL ignores these, but they're critical for mobile/WebGL.

### 4. Clamp Intermediate Values

Prevent overflow/underflow in calculations:

```glsl
// Bad
float value = pow(someInput, 0.1);

// Good
float value = pow(clamp(someInput, 0.0, 10.0), 0.1);
```

### 5. Avoid Fast Math Assumptions

Use explicit parentheses to control floating-point operation order:

```glsl
// Order-dependent
float result = ((a * b) + (c * d)) + (e * f);
```

## Common Pitfalls

### Pitfall 1: Texture LOD Calculation

```glsl
// Implicit LOD may differ
vec3 color = texture(envMap, direction).rgb;

// Explicit LOD is consistent
vec3 color = textureLod(envMap, direction, roughness * 4.0).rgb;
```

### Pitfall 2: Small Exponents in pow()

```glsl
// Very sensitive to input differences
float bad = pow(variation, 0.1);   // 10th root

// More stable
float better = sqrt(variation);     // Square root
```

### Pitfall 3: Derivatives in Divergent Branches

```glsl
// BAD: Undefined behavior
if (someCondition) {
    float dx = dFdx(value);
}

// GOOD: Compute before branching
float dx = dFdx(value);
if (someCondition) {
    // Use dx
}
```

## Testing Workflow

1. **Visual Comparison**: Test on 2+ GPU vendors
2. **Pixel Diff**: Use image comparison tools
3. **Frame Capture**: Compare with RenderDoc/ApiTrace
4. **Driver Versions**: Test with different driver releases

### Tools

```bash
# Visual diff
compare intel.png nvidia.png diff.png

# ApiTrace
apitrace trace ./app
apitrace replay app.trace
```

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

```glsl
// h = discriminant (0 at edge)
// analyticFwidth = footprint of pixel in h-space
float edgeFactor = clamp(h / analyticFwidth, 0.0, 1.0);
```

See [pbr_ibl_billboard.frag](file:///home/latty/Prog/__PERSO__/suckless-ogl/shaders/pbr_ibl_billboard.frag) for a production example.

## References

- [OpenGL Derivative Functions](https://www.khronos.org/opengl/wiki/Derivative)
- [GLSL Precision Qualifiers](https://www.khronos.org/opengl/wiki/Type_Qualifier_(GLSL)#Precision_qualifiers)
- [Floating Point Determinism](https://randomascii.wordpress.com/2013/07/16/floating-point-determinism/)
