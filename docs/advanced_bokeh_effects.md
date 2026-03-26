# Advanced Bokeh Effects: G-Master & Anamorphic Simulation

Technical research and implementation strategy for high-end optical effects in `suckless-ogl`.

## 1. Sony G-Master Lens Characteristics

Sony's G-Master (GM) line is designed to resolve high detail while maintaining a "melting" bokeh aesthetic.

### XA (Extreme Aspherical) Elements

- **Surface Precision**: 0.01 micron surface accuracy.
- **Onion Ring Suppression**: Conventional aspherical lenses often show concentric "onion rings" due to mold imperfections. GM lenses use extreme surface precision to ensure out-of-focus highlights are perfectly smooth.
- **Engine Implementation**: Historically, procedural blur or low-sample counts can introduce noise. G-Master simulation requires high-bit-depth buffers (RGBA16F) and smooth mathematical kernels (like the Kawase filter) to maintain this "clean" look.

### 11-Blade Circular Aperture

- **Shape**: Most lenses use 7 or 9 blades. 11 blades ensure that even when "stopped down" (higher f-stop), the bokeh remains almost perfectly circular.
- **Engine Implementation**: For polygonal bokeh simulation, we should support high blade counts (N=11) in the bokeh scattering pass.

---

## 2. Anamorphic Lens Effects

Anamorphic lenses use a cylindrical element to "squeeze" a wide image onto a standard sensor, which is then "desqueezed" in post-production.

### Oval Bokeh

- **Optical Cause**: The cylindrical squeeze factor (usually 1.33x, 1.5x, or 2.0x) affects the out-of-focus areas before desqueezing.
- **Effect**: Out-of-focus highlights appear as vertical ovals instead of circles.
- **Engine Implementation**: Apply an `anamorphic_ratio` (e.g., 2.0) to the blur kernel in the DOF pass.

### Waterfall Backgrounds

- **Effect**: Vertical stretching of background elements, creating a sense of height and "epic" scope.
- **Engine Implementation**: Asymmetrical blur radii (Vertical Blur > Horizontal Blur).

### Blue Streaks & Flares

- **Optical Cause**: Reflections between the cylindrical elements.
- **Engine Implementation**: A specialized Bloom pass or Flare overlay that responds to high-luminance horizontal streaks.

---

## 3. Implementation Status (POC)

### Phase 1: Symmetric vs. Asymmetric Kawase [IMPLEMENTED]

Modified the `Bloom Downsample` (13-tap) and `Bloom Upsample` (9-tap tent) shaders to support a `texelScale` factor.

- `dof_anamorphic_ratio` (default 1.0) is passed as `vec2(1.0, ratio)` to these shaders during the DoF pre-blur passes.
- This creates the characteristic vertical oval stretching of background elements before they are composited back into the main scene.

### Phase 2: UBO & Parameters [IMPLEMENTED]

Added `dof_anamorphic_ratio` to the `PostProcessUBO` and `DoFParams`.

- Synchronized between `include/postprocess.h` and `shaders/postprocess/ubo.glsl`.
- Integrated into all presets in `include/postprocess_presets.h`.
- **Sony A7S III Preset**: Now utilizes a `2.0x` anamorphic ratio by default for a distinct cinematic character.

### Phase 3: Bokeh Scattering (Optional) [PLANNED]

If a higher quality is needed, implement a scattering pass for high-luminance pixels to draw actual polygonal bokeh shapes (e.g., 11-blade polygons).
