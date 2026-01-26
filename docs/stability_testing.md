# Stability Testing & Shader Tuning

## Overview
This document details the automated stability testing framework implemented to resolve "fireflies" (flickering artifacts) and temporal instability in the PBR Billboard Shader.

## Methodology
We implemented a comparative test suite (`tests/test_stability.c`) that renders both a reference **Mesh (Icosphere)** and the target **Billboard** to measure discrepancies.

### 1. Static Fidelity Test (MSE)
Compares the static render of the Billboard against the Mesh from multiple camera angles:
-   **Center** (0,0, 2.5)
-   **Grazing Angle** (Side view)
-   **Top View**
-   **Distance** (Far view)

**Metric**: Mean Squared Error (MSE).
-   **Goal**: MSE < 0.002
-   **Result**: Achieved **~0.0011** (High Fidelity).

### 2. Temporal Stability Test (Jitter)
Simulates micro-movements of the camera (0.001 units) to detect "fireflies" or flickering pixels compared to the previous frame.

**Metric**: comparison of `Average Difference` and `Max Difference`.
-   **Average Diff**: Global stability. Should be near 0.
-   **Max Diff**: Local instability (e.g., a single flickering pixel).
    -   A value of **~1.0** at the silhouette edge is **normal** for both Mesh and Billboard due to Geometric Aliasing (no MSAA).
    -   A value of 1.0 **inside** the sphere indicates a bug (Firefly).

**Result**:
-   **Mesh Reference**: Max Diff ~0.98 (Edge Aliasing)
-   **Billboard Target**: Max Diff ~0.98 (Matches Reference)
-   **Conclusion**: The Shader is stable.

## Shader Tuning (PBR Billboard)
To achieve this stability, we applied specific damping to the `shaders/pbr_ibl_billboard.frag`:

1.  **Analytic AA (`rimRoughness`)**:
    -   Boosts roughness at grazing angles to prevent infinite frequency changes.
    -   **Tuned Value**: `smoothstep(0.1, 0.01, NdotV)`

2.  **Edge Mask (`edgeMask`)**:
    -   Masks out derivative-based normal mapping (`dFdx`) at the very edge where it becomes unstable.
    -   **Tuned Value**: `smoothstep(0.02, 0.1, clampNdotV)`

3.  **Specular Damping**:
    -   Fades out the Specular contribution (`F0`) in the last 5% of the edge to preventing sparking from unstable normals.
    -   **Tuned Value**: `smoothstep(0.0, 0.05, clampNdotV)`
