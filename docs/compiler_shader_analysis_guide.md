# Compiler & Shader Analysis Guides (July 2026)

**Date**: July 3, 2026
**Status**: Reference Guide

This guide provides concrete, step-by-step instructions for utilizing **Compiler Explorer (Godbolt.org)** and **NVIDIA Nsight Graphics (Shader Profiler)** to audit and optimize CPU and GPU execution in the `suckless-ogl` codebase.

---

## 1. Compiler Explorer (Godbolt.org)

Compiler Explorer is an interactive tool to analyze compiler assembly outputs and verify optimizations (such as AVX2 auto-vectorization, structural padding, and instruction counts).

### Scenario A: Verifying AVX2/FMA Vectorization (N-Body Physics)
To verify if the hot loop of the N-Body gravity calculations is correctly vectorized without assembly overhead:

1. **Open Godbolt**: Go to [godbolt.org](https://godbolt.org).
2. **Configure Language**: Set the source language dropdown on the left to **C**.
3. **Configure Compiler**: Set the compiler on the right to **x86-64 gcc 14.2** (or higher).
4. **Paste Code**: Copy and paste the following self-contained N-Body simulation snippet into the editor:
   ```c
   #include <math.h>

   typedef double dvec3[3];

   typedef struct {
       dvec3 position;
       dvec3 velocity;
       double mass;
       double radius;
   } Body;

   typedef struct {
       Body bodies[128];
       int body_count;
       float gravity;
   } NBodySim;

   static inline void dvec3_zero(dvec3 dest) {
       dest[0] = 0.0; dest[1] = 0.0; dest[2] = 0.0;
   }
   static inline void dvec3_sub(const dvec3 a, const dvec3 b, dvec3 dest) {
       dest[0] = a[0] - b[0]; dest[1] = a[1] - b[1]; dest[2] = a[2] - b[2];
   }
   static inline double dvec3_dot(const dvec3 a, const dvec3 b) {
       return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
   }
   static inline double dvec3_norm2(const dvec3 v) {
       return dvec3_dot(v, v);
   }
   static inline void dvec3_scale(const dvec3 v, double scalar, dvec3 dest) {
       dest[0] = v[0] * scalar; dest[1] = v[1] * scalar; dest[2] = v[2] * scalar;
   }
   static inline void dvec3_addto(dvec3 dest, const dvec3 b) {
       dest[0] += b[0]; dest[1] += b[1]; dest[2] += b[2];
   }
   static inline void dvec3_subfrom(dvec3 dest, const dvec3 b) {
       dest[0] -= b[0]; dest[1] -= b[1]; dest[2] -= b[2];
   }

   void compute_accelerations(const NBodySim* sim, double accel[][3]) {
       for (int i = 0; i < sim->body_count; i++) {
           dvec3_zero(accel[i]);
       }

       for (int i = 0; i < sim->body_count; i++) {
           for (int j = i + 1; j < sim->body_count; j++) {
               dvec3 diff;
               dvec3_sub(sim->bodies[j].position, sim->bodies[i].position, diff);

               double eps2 = 0.001; // pair softening
               double dist_sq = dvec3_norm2(diff) + eps2;
               double inv_dist = 1.0 / sqrt(dist_sq);
               double inv_dist3 = inv_dist * inv_dist * inv_dist;
               double grav = (double)sim->gravity * inv_dist3;

               dvec3 force;
               dvec3_scale(diff, grav * sim->bodies[j].mass, force);
               dvec3_addto(accel[i], force);

               dvec3_scale(diff, grav * sim->bodies[i].mass, force);
               dvec3_subfrom(accel[j], force);
           }
       }
   }
   ```
5. **Set Flags**:
   * Saisissez `-O3 -march=haswell -ffast-math` in the Compiler Options text box.
6. **Analyze Assembly**:
   * Look at the generated assembly pane. You should see instructions like `vfmadd213pd` (Fused Multiply-Add) and packed operations `vmulpd`, `vaddpd` using 256-bit `%ymm` registers.
   * Try changing the options to `-O1` or removing `-ffast-math` to see how the compiler drops down to scalar math instructions (`mulsd`, `addsd`).

---

### Scenario B: Verifying Uniform Structure Padding (UBO Alignment)
When copying structures from C to OpenGL Uniform Buffer Objects (UBOs) or Shader Storage Buffer Objects (SSBOs), memory layout must match GLSL alignment standards (`std140`/`std430`).

1. **Paste Code**:
   ```c
   #include <stddef.h>

   typedef struct {
       float exposure;
       float gamma;
       int   contrast;
   } PostProcessUniforms;

   unsigned long get_struct_size() {
       return sizeof(PostProcessUniforms);
   }

   _Static_assert(sizeof(PostProcessUniforms) == 12, "Incorrect alignment!");
   ```
2. **Observe compiler behavior**:
   * Select **x86-64 gcc 14.2** (Linux) and **x64 msvc v19.latest** (Windows).
   * Check if both compilers pass the static assertion or introduce padding bytes at the end of the structure.
   * Modify the structure members (e.g., adding `double` or `vec4` equivalents) and inspect the assembly for the returned size in `get_struct_size`.

---

### Scenario C: Offline Shader Validation (GLSL compiler)
To verify if GLSL syntax is correct and inspect compiler-independent SPIR-V assembly:

1. **Configure Language**: Set the source language dropdown on the left to **GLSL**.
2. **Configure Compiler**: Select **glslangValidator** (or **dxc**).
3. **Paste GLSL Compute Shader**:
   ```glsl
   #version 450
   layout(local_size_x = 256) in;

   layout(std430, binding = 0) buffer ParticleBuffer {
       vec4 positions[];
   };

   layout(std430, binding = 1) buffer VelocityBuffer {
       vec4 velocities[];
   };

   uniform float deltaTime;

   void main() {
       uint idx = gl_GlobalInvocationID.x;
       positions[idx] += velocities[idx] * deltaTime;
   }
   ```
4. **Inspect SPIR-V**: Review the resulting SPIR-V assembly instructions (like `OpAccessChain`, `OpFMul`, `OpFAdd`) to inspect variables and logic layout.

---

## 2. NVIDIA Nsight Graphics (Shader Profiler)

NVIDIA Nsight Graphics is a premium UI/UX GPU profiling tool. While Godbolt validates compilation, Nsight Graphics measures real hardware bottlenecks on the GPU.

### How to Profile GLSL Shaders
1. **Prepare Release Binary**:
   * Build the project using:
     ```bash
     cmake -B build-release -DCMAKE_BUILD_TYPE=Release
     cmake --build build-release --parallel
     ```
2. **Configure Nsight Graphics**:
   * Open **NVIDIA Nsight Graphics**.
   * Set **Application Executable** to the path of your compiled binary (`build-release/app`).
   * Set **Working Directory** to the root of the project.
   * Under **Activity**, select **GPU Trace** (for overall hardware bottlenecks) or **Frame Debugger** (for debugging frame drawing).
   * Click **Launch**.
3. **Capture & Analyze**:
   * Run the simulation, and click **Generate GPU Trace** (or press `F11`).
   * Once the trace finishes, navigate to the **Shader Pipelines** tab.
   * Locate the pipeline representing your compute shader (e.g. `nbody.comp`).
4. **Locating Hot Spots & Warp Stalls**:
   * Double-click the compute shader to open the **Shader Profiler** view.
   * Go to the **Hot Spots** tab. Nsight Graphics overlays the GLSL code with matching execution counters.
   * Inspect the **Top Stall** column to see why the shader is waiting:
     * `TEXTHR` (Texture Throashing/Fetch): The shader is stalled waiting for memory operations or texture cache lookups.
     * `NOTSEL` (Not Selected): Execution pipelines are busy, waiting for scheduler resources.
     * `IMCMIS` (Instruction Cache Miss): The shader code cache is missed, indicating complex branch structures.
     * `MATHATH` (Math Latency): ALU pipelines are saturated by complex math (FMA, transcendentals).

---

## 3. Local CLI Alternatives

### Assembly Generation via Just
To quickly inspect assembly locally without pushing code to Godbolt, run:
```bash
just asm src/app.c
```
This generates `src/app.c.s` locally in Release mode with native architecture flags but LTO disabled.

### Source Interleaving (`objdump`)
To view your C code side-by-side with generated assembly on your local terminal:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
objdump -S --demangle build/app | less
```
This displays C source statements directly interleaved above their compiled assembly blocks.
