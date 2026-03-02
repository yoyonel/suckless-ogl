# Specular Anti-Aliasing (Varef) Documentation

This document describes the mathematical and physical methods used for **Specular Anti-Aliasing** in the `suckless-ogl` renderer.

## 1. Problem Overview

Specular aliasing occurs when a sharp highlight (small roughness) is smaller than the pixel size. As the camera or light moves, the highlight jumps between pixels, causing "shimmering" or "fireflies." Standard MSAA usually fails here because it only affects geometric edges, not the Shading Model (NDF).

## 2. Mathematical Foundation: Varef

The renderer uses the **Varef (Variance-based Roughness Filtering)** technique. This method assumes that the microfacet distribution (NDF) and the sub-pixel geometric variation are both Gaussian distributions.

### Variance Addition

In the GGX/PBR model, the "roughness" parameter (\(r\)) is related to the variance (\(\alpha\)) of the microfacet distribution:

\[
\alpha = r^2
\]

To account for sub-pixel geometry variation (\(\sigma_g^2\)), we simply add the variances:

\[
\alpha_{total} = \alpha_{micro} + \sigma_g^2
\]

\[
\text{roughness}_{clamped} = \sqrt{\text{roughness}^2 + \sigma_g^2}
\]

## 3. Implementation Modes

The system supports two methods for estimating the geometric variance ($\sigma_g^2$):

### A. Screen-Space Mode (Derivative-based)

Used for general meshes (`u_aaMode == 0`). It uses GPU derivatives (`dFdx`/`dFdy`) of the world-space normal to estimate how much the normal rotates across the current pixel.

**Formula:**

\[
\sigma_g^2 = \text{SCALE} \cdot \max\left( \| \frac{\partial \mathbf{N}}{\partial x} \|^2, \| \frac{\partial \mathbf{N}}{\partial y} \|^2 \right)
\]

* **Pros**: Automatic, works on any mesh.
* **Cons**: Hardware-dependent (derivatives vary between vendors), prone to spikes at silhouettes.

### B. Curvature-Based Mode (Analytic)

Used for primitives with known analytic curvature, like the raytraced spheres (`u_aaMode == 1`).

**Formula:**

\[
\sigma_g^2 = \text{SCALE} \cdot \left( \frac{\text{pixelSizeWorld}}{\text{Radius}} \right)^2
\]

* **Pros**: Hardware-independent, perfectly stable at any distance, no silhouette spikes.
* **Cons**: Requires explicit knowledge of the primitive's geometry.

## 4. Tuning Constants and Factors

Found in `shaders/pbr_functions.glsl`:

| Constant | Value | Description |
| :--- | :--- | :--- |
| `SCALE` | `50.0` | Global multiplier for variance. High value for extreme stability. |
| `MAX_VARIANCE` | `0.1` | Caps the variance to prevent "exploding" roughness at silhouettes. |
| `MIN_ROUGHNESS` | `0.04` | Hard floor for roughness to avoid numerical singularities and fireflies. |
| `MAX_REFLECTION_LOD` | `4.0` | Max mip level for IBL lookups (Split-Sum Precomputed map). |

## 5. Energy Conservation

When Specular AA increases the roughness, the **Multiple Scattering Compensation** (lines 68-75 in `pbr_functions.glsl`) ensures that the lost energy is recaptured and added back to the BRDF. This prevents the surface from appearing unnaturally dark when the AA logic kicks in at distances.

## 6. Integration

* **Billboards**: Use **Curvature-mode** by default for reference-level stability.
* **Instanced**: Can use **Screen-Space mode** for general geometry, but currently defaults to a safe minimum roughness (\(0.04\)).
