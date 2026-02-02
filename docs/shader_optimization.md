# Post-Processing Shader Optimization

## Overview

The post-processing pipeline now supports two compilation modes to balance development flexibility with release performance:

1.  **Debug Mode (Dynamic)**: Features are toggled at runtime using standard uniforms (`if (enableBloom_val) ...`). This allows instant feedback when modifying settings but incurs a GPU cost for branching.
2.  **Release Mode (Static)**: Features are baked into the shader using preprocessor directives (`#define OPT_ENABLE_BLOOM 1`). The driver's GLSL compiler eliminates all unused code (Dead Code Elimination), resulting in a specialized, high-performance shader.

## Build Modes

### Debug Build (make debug / make run)
- **Flags**: `ENABLE_SHADER_OPTIMIZATION=OFF`
- **Behavior**: Shaders use dynamic uniforms. All effects are compiled, but only active ones are executed (via runtime branching, e.g. `if (enableBloom_val) ...`).
- **Use Case**: Development, tweaking parameters, debugging effect interactions.

### Release Build (make release / make run-release)
- **Flags**: `ENABLE_SHADER_OPTIMIZATION=ON`
- **Behavior**:
    - The application detects active effects at startup.
    - It generates a custom shader variant (e.g., `OPT_ENABLE_BLOOM 1`, `OPT_ENABLE_VIGNETTE 0`).
    - The GLSL compiler strips all disabled effects.
    - **Uniforms**: Textures and parameters for disabled effects are *not* set, avoiding driver validation overhead.
- **Runtime Toggling**: If you toggle an effect at runtime (e.g., press 'B'), the application **recompiles** the shader on the fly to generate a new optimized variant.
- **Use Case**: Performance profiling, production deployment.

## Technical Implementation

### 1. Shader Code (postprocess.frag)

We use a hybrid approach where uniforms are converted to `const bool` if the optimization macro is defined:

```glsl
#ifdef OPT_ENABLE_BLOOM
    // Release Mode: Constant folded by driver -> Code stripped
    const bool enableBloom_optimized = bool(OPT_ENABLE_BLOOM);
#else
    // Debug Mode: Runtime uniform -> Branching
    uniform bool enableBloom_val;
#endif
```

### 2. Startup Logic (src/app.c)

When built with `-DENABLE_SHADER_OPTIMIZATION`, the application triggers an immediate compilation of the optimized shader during initialization:

```c
#ifdef ENABLE_SHADER_OPTIMIZATION
    LOG_INFO("App", "Building specialized shader...");
    // Specialized wrapper for optimized compilation
    postprocess_compile_optimized(&app->postprocess, initial_flags);
#endif
```

### 3. Conditional Uniforms (src/postprocess.c)

To avoid OpenGL warnings (e.g., "Uniform 'bloomTexture' not found"), we guard uniform assignments. In Release mode, if Bloom is disabled, `bloomTexture` is compiled out, so we must not try to set it:

```c
// Ensure we only set uniforms that exist in the optimized shader
if (!is_optimized || (static_flags & POSTFX_BLOOM)) {
    shader_set_int(shader, "bloomTexture", UNIT_BLOOM);
}
```

## Verification

To verify the optimization works:
1.  Run `make release`.
2.  Check the logs:
    ```
    [INFO] Compiled OPTIMIZED shader with effects:
      ✓ Manual Exposure
      ✓ Color Grading
    [INFO] Shader optimization complete (Flags: 0x...)
    ```
3.  Observe that no "Uniform not found" warnings appear.
