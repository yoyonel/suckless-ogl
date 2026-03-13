# Exact Sphere AABB Optimization

This document explains the mathematical method used to calculate the optimal screen-space bounding box (AABB) for sphere rendering.

![Projective Geometry Concept](images/sphere_aabb_optimization_projective.png)

![Exact AABB Geometry](images/billboard_aabb_geometry.png)

## The Problem

When rendering spheres using ray-casting on a billboard (a generic quad), we want the quad to be as small as possible to minimize **fragment shader overdraw**.

A naive approach (projecting the center and adding the radius) fails because of **perspective distortion**. As a sphere moves to the edge of the field of view, its projection becomes an ellipse. A fixed-size quad would either be too large (wasteful) or too small (clipping the sphere).

## The Exact Solution: Tangent Planes

Instead of trying to project the sphere itself, we calculate the **viewing cone** that perfectly encompasses the sphere. This is equivalent to finding the planes passing through the camera origin (0,0,0) that are **tangent** to the sphere.

We solve this problem in 2D, independently for the X (width) and Y (height) axes.

### Geometry (XZ Plane)

Consider the top-down view (XZ plane) shown in the diagram above:

- **O**: Camera Origin at $(0,0)$.

- **C**: Sphere Center at $(C_x, C_z)$.

- **r**: Sphere Radius.

- **d**: Distance from Origin to Center ($d = \|C\|$).

We want to find the tangent lines $T_1$ and $T_2$.
The distance $L$ from the origin to the tangent points is given by Pythagoras in the right-angled triangle $\Delta OTC$:

$$ L = \sqrt{d^2 - r^2} $$

### Finding Tangent Normals (Without Trigonometry)

We can find the normal vectors of the tangent lines using simple 2D vector rotation.
The tangent lines are rotated relative to the center vector $\vec{C}$ by the angle $\alpha$.
By observing the similar triangles, we can derive the tangent direction vectors directly from $C$, $r$, and $L$:

$$ n\_{x1} = \frac{C_x \cdot L - C_z \cdot r}{d^2} $$

$$ n\_{z1} = \frac{C_z \cdot L + C_x \cdot r}{d^2} $$

(And symmetrically for the second tangent).

### Projection to NDC

Once we have the normal vector $(n_x, n_z)$ of a tangent line (which represents a ray from the camera), we project it to Normalized Device Coordinates (NDC) using the projection matrix elements.

For a standard perspective projection matrix $P$:

- $P_{00}$ scales X (based on Field of View).

- We project by dividing $x$ by $-z$ (since OpenGL looks down -Z).

$$ x*{ndc} = P*{00} \cdot \frac{n_x}{-n_z} $$

This gives us the exact screen-space coordinate of the edge of the sphere. We calculate the min/max of both tangents to find the AABB width. The same logic applies to the Y axis.

## Implementation Results

This method provides a **pixel-perfect bounding box**:

- **0% Overdraw** outside the sphere's actual screen footprint (excluding the corner areas of the quad).

- **Correct Perspective**: Handles elliptical distortion at screen edges perfectly.

- **Efficient**: Uses only square roots and basic arithmetic, avoiding expensive trigonometric functions (`acos`, `atan`).

## Robustness Handling

To ensure stability in all scenarios, two special cases are handled:

### 1. Camera Plane Singularity

When the sphere intersects the camera plane ($Z=0$), the tangent formulas can produce singularities or "wrap-around" artifacts where points behind the camera are projected inverted onto the screen.

**Solution**: If a tangent point lies behind the camera ($n_z \ge 0$), its projected screen coordinate is clamped to infinity ($\pm 10000.0$) in the correct direction. This ensures the quad extends to the screen edge.

The direction sign uses a ternary expression instead of `sign()` to avoid the GLSL `sign(0.0) = 0.0` edge case, which would collapse the bound to zero and produce a degenerate quad:

```glsl
// ❌ Before: sign(0.0) == 0.0 → bound collapses
p1 = sign(nx1) * 10000.0;

// ✅ After: explicit ternary, no zero case
p1 = (nx1 >= 0.0 ? 1.0 : -1.0) * 10000.0;

```

This singularity occurs when the tangent line is exactly axis-aligned ($n_x = 0$), which is geometrically possible for a sphere centered on the view axis.

### 2. Back-Projection Culling

Spheres located entirely behind the camera can mathematically project to valid screen coordinates (inverted).

**Solution**: These are explicitly culled in the Vertex Shader by checking if the sphere's **nearest point** along the view axis is behind the camera:

```glsl
// ❌ Before: center-only test — misses partially visible spheres
} else if (viewPos.z > 0.0) {

// ✅ After: volume-aware test — only cull when entirely behind
} else if (viewPos.z > sphereRadius) {

```

The previous test `viewPos.z > 0.0` only checked the sphere **center**. A sphere whose center is behind the camera ($z > 0$) but whose volume extends in front (e.g. center at $z = +0.5$, radius $= 2.0$) was incorrectly culled. The corrected test `viewPos.z > sphereRadius` ensures the **nearest point** of the sphere ($z_{center} - r$) is behind the camera before culling.

```text
 Camera
  ◉──────────────▶ +Z (behind)
  │
  │  ┌─────┐
  │  │  C  │  center behind (z=0.5)
  │  │  ●  │  but sphere extends in front
  │  │     │  nearest point = z - r = -1.5

  │  └─────┘

```

### 3. Conservative Depth

To ensure correct Z-buffering when the sphere intersects other geometry (e.g. a wall or floor passing through it), the billboard quad is positioned at the sphere's **frontmost plane** ($Z_{nearest} = Z_{view} + R$) rather than its center.

This ensures the quad is drawn _before_ any intersecting geometry that might be inside the sphere, safeguarding against incorrect occlusion. The Fragment Shader then outputs the precise per-pixel depth (`gl_FragDepth`) to carve out the true spherical shape.

The nearest Z is clamped to stay in front of the near plane. The near plane distance is now **derived dynamically** from the projection matrix instead of being hardcoded:

```glsl
// ❌ Before: magic number coupled to CPU-side NEAR_PLANE = 0.1
nearestZ = min(nearestZ, -0.11);

// ✅ After: extracted from the projection matrix
float zNear = projection[3][2] / (projection[2][2] - 1.0);

nearestZ = min(nearestZ, -(zNear + 0.01));

```

For a standard OpenGL perspective matrix:

$$
P = \begin{pmatrix}
sx & 0 & 0 & 0 \\
0 & sy & 0 & 0 \\
0 & 0 & A & B \\
0 & 0 & -1 & 0
\end{pmatrix}
\quad\text{where}\quad
A = P_{22} = -\frac{f+n}{f-n},\quad
B = P_{32} = -\frac{2fn}{f-n}
$$

Solving for the near plane:

$$
z_{near} = \frac{B}{A - 1} = \frac{P_{32}}{P_{22} - 1}
$$

This eliminates the coupling between the shader and the CPU-side `NEAR_PLANE` constant, making the code robust to near plane changes.

### 4. Numerical Stability: Avoiding Silhouette Jitter

When using ray-casting on billboards, it is critical that attributes constant across the sphere (center, radius, material) are passed using the **`flat`** interpolation qualifier.

By default, OpenGL performs perspective-correct interpolation. Even if all four vertices of a billboard share the same value (e.g., $Radius = 1.0$), floating-point precision errors during interpolation can cause values to fluctuate slightly across the quad (e.g., $0.9999999$ or $1.0000001$). At the sphere's silhouette, where the intersection test is highly sensitive (discriminant near zero), these micro-variations produce **"rainbow dots"** or noise artifacts.

Using `flat out` in the Vertex Shader and `flat in` in the Fragment Shader ensures bit-perfect consistency by disabling interpolation. Instead, the value is taken from a single vertex (the **Provoking Vertex**), guaranteeing "ISO" rendering results identical to geometric spheres.

- **See also**: [Flat Interpolation (Khronos Wiki)](<https://www.khronos.org/opengl/wiki/Type_Qualifier_(GLSL)#Interpolation_qualifiers>) and [Provoking Vertex (Khronos Wiki)](https://www.khronos.org/opengl/wiki/Primitive#Provoking_vertex).

### 5. Inside-Sphere Epsilon Robustness

When the camera is inside or very near the sphere surface, the shader switches to a full-screen quad for ray-casting. The detection threshold uses an **additive + multiplicative** epsilon to handle all sphere scales:

```glsl
// ❌ Before: purely multiplicative — fails for tiny spheres
if (distSq <= r2 * 1.005)

// ✅ After: additive floor ensures a minimum absolute margin
if (distSq <= r2 + max(r2 * 0.005, 1e-4))

```

| Sphere Radius | Old Margin ($r^2 \times 0.005$) | New Margin ($\max(r^2 \times 0.005,\; 10^{-4})$) |
| :-----------: | :-----------------------------: | :----------------------------------------------: |
|  $r = 10.0$   |              $0.5$              |                      $0.5$                       |
|   $r = 1.0$   |             $0.005$             |                     $0.005$                      |
|  $r = 0.01$   |       $5 \times 10^{-7}$        |                   $10^{-4}$ ✅                   |
|  $r = 0.001$  |       $5 \times 10^{-9}$        |                   $10^{-4}$ ✅                   |

For large spheres, the multiplicative term dominates. For very small spheres (e.g. particle systems), the additive floor $10^{-4}$ prevents false-negative detection caused by float32 precision limits.

### 6. Code Structure: Shared Projection Scale Factors

The projection matrix diagonal elements `sx = projection[0][0]` and `sy = projection[1][1]` are used in both the inside-sphere and normal projection branches. They are now extracted **once** before the branch chain, eliminating code duplication and making the dependency explicit.

### 7. The Mesh vs. Math Paradox (Understanding Diff Maps)

When comparing this optimized billboard rendering to a traditional triangle-based sphere (Reference), a "diff map" will often show persistent colored rings around the silhouettes. This is **expected** and proves the accuracy of the mathematical approach:

1. **Perfect Silhouette:** The billboard computes a mathematically perfect curve for every pixel. An icosphere, regardless of subdivision, is an approximation made of flat triangles. The rings represent the areas where the triangle edges deviate from the perfect sphere.

2. **Anti-Aliasing Clash:** Our shader uses **Analytical Edge Smoothing** (Soft Edges). Standard mesh rendering uses hardware rasterization. The transition from 100% to 0% opacity differs slightly, producing residuals in difference maps.

3. **Normal Continuity:** Ray-casted normals are perfectly continuous, while mesh normals (even when smoothed) depend on the underlying vertex interpolation.

## References

- **Mara, M., McGuire, M., & Luebke, D. (2013).** _2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere_. Journal of Computer Graphics Techniques (JCGT).

  [PDF Reference](https://jcgt.org/published/0002/02/05/)

See `shaders/pbr_ibl_billboard.vert` for the GLSL implementation.

### Visual Illustration

The following image (from Mara et al.) demonstrates how the spherical projection creates an elliptical footprint on the screen, which our exact AABB calculation perfectly bounds:

![Perspective Projection Grid](images/perspective_projection_grid.png)

---

## Changelog

### 2026-02-08 — Robustness Audit & Fixes

Five corrections applied to `projection_utils.glsl` following a mathematical audit of `computeBillboardSphere`:

| # | Severity | Fix | Section |

| --- | ---------- | ----- | --------- |

| 1 | **Bug** | Behind-camera cull: `viewPos.z > 0.0` → `viewPos.z > sphereRadius` — spheres straddling the camera plane are no longer incorrectly culled | §2 |

| 2 | **Robustness** | Near plane clamp derived from projection matrix instead of hardcoded `-0.11` — decoupled from CPU-side `NEAR_PLANE` | §3 |

| 3 | **Robustness** | `sign(nx)` → `(nx >= 0.0 ? 1.0 : -1.0)` — prevents `sign(0)=0` bound collapse | §1 |

| 4 | **Quality** | Inside-sphere epsilon: `r² × 1.005` → `r² + max(r² × 0.005, 1e-4)` — robust at all sphere scales | §5 |

| 5 | **Style** | `sx`/`sy` hoisted before branch chain, removing duplication | §6 |
