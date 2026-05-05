# N-Body Numerical Stability Fix

## Problem Description
Numerical instability was observed in the N-body simulation, occasionally leading to `NaN` (Not a Number) values in body velocities or highly erratic trajectories. These issues typically stemmed from floating-point optimization side effects and unnormalized initial velocity vectors.

## Technical Causes

### 1. Fast-Math Optimizations
Compilers often use `-ffast-math` or similar aggressive optimizations to improve performance. However, these optimizations:
- Sacrifice strict IEEE 754 compliance.
- Allow reassociation of mathematical operations (e.g., `(a + b) + c` becomes `a + (b + c)`), which can lead to significant precision loss in iterative physics simulations.
- Assume no `NaN` or `Inf` values exist, leading to undefined behavior when they do occur (e.g., during close body encounters where forces are extremely high).

### 2. Unnormalized Velocity Directions
During initialization in `nbody_init_preset`, the orbital velocity was calculated by scaling a direction vector (`orb->vel_dir`). If this vector was not unit length, the resulting velocity would be incorrectly scaled, potentially leading to extreme speeds and immediate simulation divergence.

## Solution

### Disabling Fast-Math for Physics
The simulation core in `src/nbody.c` now explicitly disables fast-math optimizations using a compiler pragma:
```c
#pragma GCC optimize ("no-fast-math")
```
This ensures that the Velocity Verlet integrator maintains maximum precision and handles numerical edge cases (like small distances or NaNs) correctly according to standard floating-point rules.

### Velocity Direction Normalization
The initialization logic now ensures that the velocity direction vector is normalized before being scaled by the orbital speed:
```c
vec3 normalized_vel_dir;
glm_vec3_copy((float*)orb->vel_dir, normalized_vel_dir);
glm_vec3_normalize(normalized_vel_dir);

vec3 vel = {normalized_vel_dir[0] * spd, normalized_vel_dir[1] * spd,
            normalized_vel_dir[2] * spd};
```

### NaN Detection
A diagnostic check has been added to the integration step to print a warning if a body's velocity becomes `NaN`, allowing for easier debugging of future stability issues.
