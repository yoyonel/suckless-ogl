# Technical Glossary

This glossary covers technical terms and expressions used in the project, spanning theoretical PBR rendering aspects, geometric optimizations, and low-level graphics API concepts.

---

## 🎨 Rendering & Physics (PBR / IBL)

| Term | Software / Theoretical Definition |
| :--- | :--- |
| **PBR** | *Physically Based Rendering*. A rendering model grounded in the laws of physics to simulate how light realistically interacts with materials. |
| **IBL** | *Image Based Lighting*. Using an image (HDR cubemap) to simulate complex global illumination. |
| **BRDF** | *Bidirectional Reflective Distribution Function*. A function defining how a material reflects light (Cook-Torrance model). |
| **NDF (GGX)** | *Normal Distribution Function*. The component of PBR describing the micro-geometry of surfaces (microfacet distribution). |
| **Split-Sum** | A mathematical approximation (Epic Games) enabling real-time IBL specular computation via pre-integration (Prefiltered Map + BRDF LUT). |
| **Irradiance / Radiance** | Irradiance is the total incident luminous flux (diffuse); Radiance is the flux in a specific direction (specular). |
| **Normal Mapping / TBN** | Detail simulation via a texture. The **TBN** matrix (Tangent, Bitangent, Normal) transforms vectors from Tangent Space to World Space. |

## 📐 Projection Optimizations (Spheres / Billboards)

| Term | Software Definition |
| :--- | :--- |
| **Impostors** | A technique simulating complex 3D geometry (sphere) on a 2D quad via ray-casting in the shader. |
| **Analytic AA** | Mathematical anti-aliasing on the sphere edge, computed from the discriminant derivative (`fwidth`), yielding perfect contours. |
| **Tangent Planes** | A geometric method computing the perfect screen-space bounding box (AABB) of a sphere via tangent planes passing through the camera. |
| **Conservative Depth** | Positioning the quad at the sphere's closest point to guarantee a correct Z-test before writing `gl_FragDepth`. |
| **Discriminant** | The ray-sphere equation value ($b^2 - ac$). Determines whether a pixel is inside ($>0$) or outside ($<0$) the sphere. |
| **Perspective Distortion** | Elliptical deformation of a sphere when it moves away from screen center, handled here by exact tangent projection. |

## ⚙️ Graphics API & Data Flow (GPU)

| Term | Software Definition |
| :--- | :--- |
| **PBO (Zero-Copy)** | *Pixel Buffer Object*. Using `GL_MAP_UNSYNCHRONIZED_BIT` to upload data without stalling the CPU. |
| **Fence Sync** | A `glFenceSync` object used to check GPU task completion without forcing a full pipeline flush (stall). |
| **Flat Interpolation** | The `flat` qualifier preventing interpolation between vertices, critical for numerical stability of silhouette calculations on spheres. |
| **Provoking Vertex** | The vertex whose value is used for the entire primitive during `flat` interpolation. |
| **Pipeline Stall** | A critical latency where the CPU waits for the GPU (caused by synchronous reads or `glFinish`). |

## 🚀 Architecture & System

| Term | Software Definition |
| :--- | :--- |
| **Time Slicing** | Splitting heavy operations (IBL) into small slices spread across multiple frames to maintain a constant FPS. |
| **Double Buffering (Pending)** | Preparing a new state (e.g., environment) in the background while the old one is still being displayed. |
| **SIMD (AVX)** | Using wide processor instructions to process multiple floats simultaneously (used for sphere sorting). |
| **PAL** | *Platform Abstraction Layer*. A layer isolating business logic from system-specific details (Linux/Windows). |
| **MkDocs / Doxygen** | Documentation generation tools (Narrative vs. API Reference). |
| **Tracy** | A hybrid profiler (CPU/GPU) used to analyze performance in real time. |
