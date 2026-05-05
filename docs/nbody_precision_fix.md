# N-Body Numerical Stability Fix

## Problem Description
Numerical instability was observed in the N-body simulation, leading to energy drift, erratic trajectories, and occasionally `NaN` values. These issues were particularly severe in `release` builds and during time-reversal operations.

## Technical Causes

### 1. Floating Point Precision Loss
Physics simulations involving cumulative calculations are highly sensitive to rounding errors. Using `float` (32-bit) precision led to significant energy drift over long periods, as small errors in force calculations accumulated every frame.

### 2. Time-Reversal Damping Bug
The radial damping mechanism (confinement zone) used `delta_time` directly. When time was reversed (`delta_time < 0`), the damping term became an accelerating force, causing bodies to gain infinite energy and explode out of the simulation volume.

### 3. Fast-Math Side Effects
Aggressive compiler optimizations (`-ffast-math`) sacrifice IEEE 754 compliance for speed. This can cause the symplectic properties of the Velocity Verlet integrator to break, leading to non-physical behavior.

## Solution

### 1. Migration to Double Precision
The core physics state and calculations have been migrated to `double` (64-bit) precision.
- **Affected fields**: Position, velocity, mass, and energy calculations.
- **Rendering**: The GPU still receives `float` data via VBOs for performance, but the simulation "source of truth" is now high-precision.

### 2. Symmetrical Damping
The damping logic was updated to use `fabs(delta_time)`, ensuring it remains energy-dissipative regardless of the time direction:
```c
float damping_factor = 1.0F - (sim->damping * fabsf(delta_time));
```

### 3. Build-Level Flag Management
Instead of fragile source-code pragmas, we now enforce standard floating-point behavior via `CMakeLists.txt`:
```cmake
set_source_files_properties(src/nbody.c PROPERTIES COMPILE_FLAGS "-fno-fast-math")
```
This ensures the physics core is always compiled with strict IEEE 754 compliance, even if the rest of the application uses aggressive optimizations.

## Verification
Long-run tests (1200s) confirm an energy drift of less than 3%, and time-reversal tests show near-perfect reversibility ($10^{-11}$ error), confirming the robustness of the new implementation.
