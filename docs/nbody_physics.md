# N-Body Gravity Simulation — Physics Reference

## Overview

The N-body module (`nbody.h` / `nbody.c`) implements a real-time gravitational
simulation of up to 16 bodies orbiting a central star.  The physics engine
prioritises **long-term stability** (energy conservation over thousands of
simulated seconds) using well-established techniques from computational
astrophysics:

| Component | Technique |
|-----------|-----------|
| Integrator | **Velocity Verlet** (symplectic, 2nd-order) |
| Regularisation | **Plummer softening** (per-pair, adaptive) |
| Initial conditions | **Softened orbital velocity** formula |
| Timestep | Fixed $\frac{1}{120}\text{s}$, accumulator-clamped |

Validated by a 1 200 s automated test with **0.002 %** energy drift.

---

## Velocity Verlet Integrator

[Velocity Verlet](https://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet)
(also called *Leapfrog kick-drift-kick*) is a **symplectic** integrator: it
preserves the geometric structure of Hamiltonian systems, which guarantees that
total energy oscillates around the true value instead of drifting monotonically.

### Algorithm (per fixed step $\Delta t$)

1. Compute accelerations from current positions: $\mathbf{a}_\text{old}$
2. Update positions:
$$\mathbf{x} \leftarrow \mathbf{x} + \mathbf{v}\,\Delta t + \tfrac{1}{2}\,\mathbf{a}_\text{old}\,\Delta t^2$$
3. Compute accelerations from new positions: $\mathbf{a}_\text{new}$
4. Update velocities:
$$\mathbf{v} \leftarrow \mathbf{v} + \tfrac{1}{2}(\mathbf{a}_\text{old} + \mathbf{a}_\text{new})\,\Delta t$$

This is implemented in `integrate_step()` in `src/nbody.c`.

### Why Symplectic Matters

Non-symplectic methods (Euler, RK4) accumulate a secular energy drift
proportional to the number of steps.  Over 1 200 s at 120 Hz that is 144 000
steps — enough for even small per-step errors to blow the system apart or
collapse it.  A symplectic integrator's bounded energy error keeps the orbits
qualitatively correct indefinitely.

**References:**

- [Symplectic integrator — Wikipedia](https://en.wikipedia.org/wiki/Symplectic_integrator)
- [Leapfrog integration — Wikipedia](https://en.wikipedia.org/wiki/Leapfrog_integration)
- Hairer, Lubich & Wanner, *Geometric Numerical Integration* (Springer, 2006)

---

## Plummer Softening

In a pure $1/r^2$ gravitational field, two point masses passing very close
experience arbitrarily large forces, leading to numerical slingshot effects and
chaotic energy exchange.  The standard astrophysical cure is
[Plummer softening](https://en.wikipedia.org/wiki/Plummer_model):

$$F = \frac{G\,m_i\,m_j}{(r^2 + \varepsilon^2)^{3/2}}\,\hat{\mathbf{r}}$$

The softening length $\varepsilon$ regularises the singularity at $r = 0$,
turning the Newtonian $1/r$ potential into the finite
[Plummer potential](https://en.wikipedia.org/wiki/Plummer_model)
$\Phi = -G\,M / \sqrt{r^2 + \varepsilon^2}$.

### Per-Pair Adaptive Softening

Rather than a single global $\varepsilon$, we scale the softening to each
body pair's physical radii:

$$\varepsilon^2 = \max\!\bigl(\texttt{NBODY\_SOFTENING\_SQ},\;
      (\texttt{NBODY\_SOFTENING\_FACTOR} \cdot (r_i + r_j))^2\bigr)$$

With the default constants:

| Constant | Value | Purpose |
|----------|-------|---------|
| `NBODY_SOFTENING_SQ` | 0.25 | Absolute floor ($\varepsilon_\min = 0.5$) |
| `NBODY_SOFTENING_FACTOR` | 2.0 | Per-pair multiplier on combined radii |

This ensures that:

- Large bodies (the star, $r = 1.5$) get strong softening ($\varepsilon \geq 3.6$).
- Small pairs (two moons) get proportionally less, keeping their dynamics
  more Keplerian.
- No pair ever reaches the singular region.

Implemented in `pair_softening_sq()`.

**References:**

- [Gravitational softening — Wikipedia](https://en.wikipedia.org/wiki/Softening)
- Dehnen & Aly, *Improving convergence of N-body methods* (MNRAS 425, 2012)
- Aarseth, *Gravitational N-Body Simulations* (Cambridge, 2003)

---

## Softened Orbital Velocity

A common mistake is to initialise circular-orbit speeds using the Kepler
formula $v = \sqrt{G\,M/r}$.  This is **wrong** when softening is active
because the effective potential is shallower than $1/r$.

The correct circular-orbit condition for the Plummer potential is:

$$v = r \cdot \sqrt{\frac{G\,M}{(r^2 + \varepsilon^2)^{3/2}}}$$

This comes from balancing the centripetal acceleration $v^2/r$ with the
softened gravitational acceleration $G\,M\,r / (r^2 + \varepsilon^2)^{3/2}$.

Using the unsoftened formula over-estimates the orbital speed, injecting
excess kinetic energy and causing bodies to spiral outward.

Implemented in `softened_orbital_vel()`.

---

## Momentum Zeroing

After initialising all bodies, the total momentum $\mathbf{p} = \sum m_i \mathbf{v}_i$
is set to zero by subtracting the centre-of-mass velocity from every body:

$$\mathbf{v}_i \leftarrow \mathbf{v}_i - \frac{\sum m_j\,\mathbf{v}_j}{\sum m_j}$$

This keeps the centre of mass stationary and prevents the whole system from
drifting across the scene.  It is done **once** at init, not per step (which
would break symplecticity).

---

## Fixed Timestep with Accumulator

Variable timesteps destroy the symplectic property.  The simulation uses a
fixed $\Delta t = 1/120\,\text{s}$ with a standard accumulator pattern:

```text
accumulator += wall_dt × time_scale
clamp accumulator to NBODY_MAX_ACCUMULATOR (1/10 s)
while accumulator >= NBODY_FIXED_DT:
    integrate_step(NBODY_FIXED_DT)
    accumulator -= NBODY_FIXED_DT
```

The max-accumulator clamp prevents a spiral of death after lag spikes or on
the first frame.  The ceiling of $1/10\,\text{s}$ supports frame rates as
low as ~10 FPS without losing simulation time (up to 12 Verlet steps per
frame for $N = 14$ bodies — trivially cheap on the CPU).

---

## Approaches Tried and Rejected

During development, several alternative techniques were tested and
**rejected** because they broke the symplectic invariant or introduced
unacceptable energy dissipation.

### 1. Elastic Collisions

**Idea:** When two bodies overlap ($r < r_i + r_j$), reflect their velocities
using elastic collision formulae.

**Problem:** Discrete collision detection at fixed timestep introduces
non-time-reversible impulses.  This breaks symplecticity and causes secular
energy drift.  In practice, bodies gained energy at every collision,
leading to exponential blow-up.

**Verdict:** Removed entirely.  Plummer softening prevents overlap without
the need for collision handling.

### 2. Energy Clamp

**Idea:** After each step, measure total energy; if it exceeds a threshold,
rescale all velocities to restore the target energy.

**Problem:** This is a projection step that is not derived from a Hamiltonian.
It destroys the symplectic structure and introduces artificial damping
or pumping depending on the sign of the correction.

**Verdict:** Removed.

### 3. Escape Brake

**Idea:** If a body moves beyond a radius threshold, apply a drag force to
bring it back.

**Problem:** Non-conservative force → secular energy loss → orbits decay
and system collapses.

**Verdict:** Removed.

### 4. Per-Step Momentum Re-Zeroing

**Idea:** Subtract the centre-of-mass velocity from all bodies every step.

**Problem:** This is a non-symplectic projection applied at every step.  It
interferes with the integrator's phase-space structure and causes energy
to drift downward.

**Verdict:** Momentum is zeroed **once** at init, never during simulation.

### 5. XPBD (Extended Position-Based Dynamics)

**Idea:** Use XPBD constraint solver (from game physics) to enforce minimum
distance between bodies, replacing softening with constraint-based repulsion.

**Problem:** XPBD is **fundamentally dissipative by design** — it solves
constraints by projecting positions, which does not conserve energy.
In testing, this caused **21 % energy loss** over 300 s, orbits decayed
rapidly, and all bodies collapsed onto the star.

XPBD is excellent for game-physics scenarios (ragdolls, cloth) where
dissipation is acceptable and even desirable, but it is **catastrophic**
for gravity simulations that require energy conservation.

**Verdict:** Rejected after testing.  Pure Verlet + strong Plummer softening
is the correct approach for N-body gravity.

**References:**

- Müller et al., *Detailed Rigid Body Simulation with Extended Position Based Dynamics* (CGF 2020)
- [Position Based Dynamics — Wikipedia](https://en.wikipedia.org/wiki/Position-based_dynamics)

---

## Confinement Potential

Bodies that drift too far from the central star are pushed back by a
**quadratic restoring potential** — a soft wall that keeps the simulation
visually bounded without hard-clamping positions.

### Physics

For each body $i$ at distance $r_i$ from the star:

$$V_\text{conf}(r) = \begin{cases}
0 & r \le r_\text{max} \\
\tfrac{1}{2} k (r - r_\text{max})^2 & r > r_\text{max}
\end{cases}$$

The resulting force is:

$$\mathbf{F}_\text{conf} = -k\,(r - r_\text{max})\,\hat{\mathbf{r}}$$

where $r_\text{max}$ = `NBODY_CONFINEMENT_RADIUS` (25 units) and $k$ =
`NBODY_CONFINEMENT_K` (5.0).

**Newton's 3rd law reaction** on the central star preserves centre-of-mass:

$$\mathbf{F}_\text{star} = +k\,(r - r_\text{max})\,\hat{\mathbf{r}} \times \frac{m_i}{m_0}$$

The potential is conservative, so the Velocity Verlet integrator remains
symplectic inside the confinement zone.

### Constants

| Constant | Value | Role |
|----------|-------|------|
| `NBODY_CONFINEMENT_RADIUS` | 25.0 | Soft wall distance from star |
| `NBODY_CONFINEMENT_K` | 5.0 | Spring stiffness ($k \cdot dt^2 \approx 0.0003$) |

---

## Proportional Radial Damping

The confinement potential alone causes bodies to oscillate around the
boundary (ping-pong trajectories).  A **proportional radial damping** term
removes energy only from the outward velocity component, and only in the
confinement zone:

$$\gamma_\text{eff} = \gamma \cdot \frac{r - r_\text{max}}{r_\text{max}}$$

where $\gamma$ = `NBODY_CONFINEMENT_DAMPING` (20.0).

- **Radial only**: only the $(\mathbf{v} \cdot \hat{\mathbf{r}})$ component
  is damped; tangential speed is preserved → angular momentum is conserved.
- **Outward only**: damping is applied when $v_\text{radial} > 0$ (body
  moving away from star).
- **Proportional to overshoot**: near the boundary ($r \approx r_\text{max}$),
  damping is negligible; far beyond, it is strong.  This lets the system
  reach an equilibrium where orbits no longer cross the boundary and energy
  drain stops (drift plateaus).
- **Momentum conservation**: the impulse is transferred to the central star
  ($\Delta v_\text{star} = -(m_i/m_0) \cdot \Delta v_i$).

### Stability Indicator

The HUD stability indicator uses **signed** energy drift to distinguish
damping from divergence:

| Colour | State | Condition | Display |
|--------|-------|-----------|---------|
| Green | Stable | $|\text{drift}| < 5\%$ | `drift: X.XX%` |
| Yellow | Damping | $\text{drift}_\text{signed} < 0$ | `E: XX%` (retained) |
| Red | Divergent | $\text{drift}_\text{signed} > 0$ | `drift: X.XX%` |

In **Damping** mode, the display shows `E: XX%` = $100 \times (1 - |\text{drift}|)$,
representing how much of the initial energy the system still has.  This avoids
the confusing "drift: 349%" that would occur with the absolute metric.

---

## Shockwave Visual Effects

When a body impacts the confinement boundary, a **shockwave ring** expands
from the impact point, providing visual feedback for the otherwise invisible
confinement zone.

### Impact Detection

Each integration sub-step checks whether a body's distance from the star
exceeds `NBODY_CONFINEMENT_RADIUS` with a positive outward radial velocity.
Impacts are recorded per-body in `NBodyImpact` slots — only the **peak
velocity** across all sub-steps of a frame is kept, naturally deduplicating
multiple hits from the fixed-timestep accumulator (~12 sub-steps/frame).

### Rendering

Shockwaves are rendered as **additive-blended screen-aligned quads** (billboard)
with a procedural expanding ring pattern in the fragment shader:

- **Ring profile**: `smoothstep` inner/outer edges around a time-dependent
  centre radius
- **HDR emissive**: output colour is scaled by 4× for bloom interaction
- **Fade**: intensity = $(1 - \text{progress}^2)$ for quick ramp-up, slow fade
- **Billboard**: quad faces camera using right/up vectors extracted from the
  view matrix

Up to `SHOCKWAVE_MAX_ACTIVE` (8) simultaneous shockwaves; oldest is evicted
when full.  Duration is `SHOCKWAVE_DURATION` (1.2 s), max ring radius is
`SHOCKWAVE_MAX_RADIUS` (6.0 world units).

### Pipeline Position

Shockwaves are drawn in the **HDR FBO**, after N-body trails and before
post-processing.  This means bloom naturally amplifies the ring glow:

```text
scene_render() → Skybox → Spheres → NBody Trails → Shockwave VFX → Probes
                                                      ↓
                                              postprocess_end() → Bloom → ...
```

---

## Test Validation

The automated test suite (`tests/test_nbody_stability.c`) verifies physics
invariants over long simulation runs:

| Test | What it checks | Threshold |
|------|---------------|-----------|
| `test_nbody_single_step_sanity` | One step does not explode | Positions finite |
| `test_nbody_energy_conservation` | Energy bounded after init boost | $\Delta E / E_0 < 65\%$ |
| `test_nbody_paused_no_change` | Paused sim is perfectly frozen | Bit-exact |
| `test_nbody_survives_dt_spikes` | Spike frames do not destabilise | All bodies $< 50$ units |
| `test_nbody_long_run_stability` | 1 200 s simulation invariants | See below |
| `test_nbody_kinetic_energy_positive` | $E_k > 0$ after init | Positive |
| `test_nbody_energy_drift_api` | Drift API: $E_0$ stored, drift $\approx 0$ at $t=0$ | drift $< 5\%$ after 10 s |
| `test_nbody_zero_gravity` | $G = 0$ means no acceleration | Velocity unchanged after 120 steps |

### Long-Run Stability Results (1 200 s at 120 Hz = 144 000 steps)

| Metric | Measured | Threshold |
|--------|----------|-----------|
| Energy drift $\|\Delta E / E_0\|$ | 0.0017 % | < 5 % |
| Centre-of-mass drift | 0.001 | < 0.5 |
| Maximum body distance | 19.6 | < 50 |
| Body count | 7 / 7 | unchanged |

---

## Constants Reference

All constants are defined in `include/nbody.h`:

| Constant | Value | Description |
|----------|-------|-------------|
| `NBODY_MAX_BODIES` | 16 | Maximum body count |
| `NBODY_DEFAULT_G` | 1.0 | Gravitational constant |
| `NBODY_SOFTENING_SQ` | 0.25 | Minimum $\varepsilon^2$ |
| `NBODY_SOFTENING_FACTOR` | 2.0 | Per-pair softening multiplier |
| `NBODY_FIXED_DT` | 1/120 s | Fixed integration timestep |
| `NBODY_MAX_ACCUMULATOR` | 1/10 s | Max accumulated physics time (supports ~10 FPS) |
| `NBODY_CONFINEMENT_RADIUS` | 25.0 | Soft wall distance from central star |
| `NBODY_CONFINEMENT_K` | 5.0 | Confinement spring stiffness |
| `NBODY_CONFINEMENT_DAMPING` | 20.0 | Radial damping base coefficient |
| `SHOCKWAVE_MAX_ACTIVE` | 8 | Max simultaneous shockwaves |
| `SHOCKWAVE_DURATION` | 1.2 s | Shockwave ring lifetime |
| `SHOCKWAVE_MAX_RADIUS` | 6.0 | Max ring expansion (world units) |
| `SHOCKWAVE_MIN_VELOCITY` | 0.2 | Min velocity to trigger shockwave |
| `TRAIL_DURATION_DEFAULT` | 4.0 s | Trail lifetime in seconds (adjustable 0.5–30 s) |

---

## Runtime Controls

### Simulation Speed (`time_scale`)

The simulation speed can be adjusted at runtime via keyboard:

| Key (US layout) | Key (AZERTY) | Action |
|-----------------|--------------|--------|
| `.` | `:` | Double speed (max 64×) |
| `,` | `;` | Halve speed (min 1/8×) |

The `time_scale` multiplier follows powers of 2 to ensure that `1.0×` is
always reachable:

$$\text{time\_scale} \in \{0.125,\; 0.25,\; 0.5,\; 1.0,\; 2.0,\; 4.0,\; 8.0,\; 16.0,\; 32.0,\; 64.0\}$$

An overlay notification displays the current speed on each change.

### Time Reversal

The time direction can be **reversed** at runtime:

| Key (US layout) | Key (AZERTY) | Action |
|-----------------|--------------|--------|
| `Ctrl+Shift+G` | `Ctrl+Shift+G` | Toggle time direction (forward/reverse) |

This exploits a fundamental property of Newtonian gravity: the equations of
motion are **invariant under $t \to -t$**.  Reversing time is equivalent to
negating all velocities — the system retraces its trajectory backwards.

Because Velocity Verlet is both **symplectic** and **time-reversible**
($\Phi_h^{-1} = \Phi_{-h}$), the reversed simulation follows the same
orbit with bounded energy error.  After $N$ forward steps and $N$ backward
steps, the system returns to the initial state (modulo floating-point
rounding: $\sim N \cdot \epsilon_{\text{machine}}$ position error).

When reversed, the HUD shows a ⏪ indicator next to the energy readout.
Speed controls (`,` / `.`) adjust the magnitude without affecting the
time direction.

#### Progressive Transition

The reversal is not instantaneous: `time_scale` ramps linearly toward
its target at a rate of **3.0 units/s** (`NBODY_TIME_SCALE_RATE`).  At
$1\times$ speed the full forward→reverse transition takes ~0.7 s:

$$+1.0 \xrightarrow{\text{decelerate}} 0.0 \xrightarrow{\text{accelerate}} -1.0$$

This produces a natural “VHS rewind” feel where bodies slow down, pause
briefly, then accelerate in reverse.  Higher speed multipliers take
proportionally longer to reverse (e.g. $4\times$ → ~2.7 s).

### Gravity Control (`gravity`)

The gravitational constant $G$ can be adjusted at runtime with Shift modifier:

| Key (US layout) | Key (AZERTY) | Action |
|-----------------|--------------|--------|
| `Shift+.` | `Shift+:` | Double G (max 128) |
| `Shift+,` | `Shift+;` | Halve G (min 0.125, then OFF) |

$G$ follows powers of 2 from the current value:

$$G \in \{0,\; 0.125,\; 0.25,\; 0.5,\; 1.0,\; 2.0,\; \ldots,\; 128.0\}$$

- **$G = 0$** disables gravity entirely: bodies follow straight-line ballistic
  trajectories at their current velocity.
- When $G$ changes, the reference energy $E_0$ is **recalculated** so the
  stability indicator reflects only numerical integration error, not the
  intentional physics parameter change.

### HUD Overlay

When the N-body mode is active and text overlay is enabled (`F1`), two lines
are displayed:

1. **Energy & Gravity:** `Ek: 77.1 J | G: 2.000` — kinetic energy
   $E_k = \sum \tfrac{1}{2} m_i |\mathbf{v}_i|^2$ and current $G$ value.
2. **Stability:** colour-coded indicator with context-dependent metric:

| Colour | State | Metric | Example |
|--------|-------|--------|---------|
| Green | Stable | drift% | `Stability: Stable (drift: 0.12%)` |
| Yellow | Damping | retained energy | `Stability: Damping (E: 78%)` |
| Red | Divergent | drift% | `Stability: Divergent (drift: 6.42%)` |

Drift is $|E(t) - E_0| / |E_0|$.  In Damping mode, retained energy is
$100 \times (1 - \text{drift})$, showing how much energy the system still has
(avoids confusing >100% drift values from long damping runs).

### Trail Length Stability — Time-Based Sampling

Trail length is controlled by a **duration parameter** (`trail_duration`,
default 4.0 s, adjustable 0.5–30 s) rather than a fixed point count.  Each
recorded sample carries a **timestamp** from a monotonic simulation clock:

```text
sim_time += wall_dt × |time_scale|
sample_timer += wall_dt × |time_scale|
while sample_timer >= TRAIL_SAMPLE_INTERVAL:
    for each body:
        ring_push(position, sim_time)   ← timestamped
    sample_timer -= TRAIL_SAMPLE_INTERVAL
```

The ribbon builder then computes per-point age:

$$\text{age} = \frac{\text{sim\_time} - \text{timestamp}}{\text{trail\_duration}}$$

Points with $\text{age} > 1$ are discarded.  Width and intensity taper use
this age factor instead of the point's ring-buffer index.

**Why this matters at low FPS:**  At 15 FPS only ~15 samples/sec are recorded
(fewer unique positions), but each carries the correct timestamp.  The ribbon
still spans exactly `trail_duration` seconds of trajectory, so trail length
is identical at 15 and 60 FPS.  At high `time_scale` (e.g. 8×), 8 samples
are emitted per frame, preserving spatial fidelity.

---

## Profiling Zones

The N-body pipeline is instrumented with fine-grained CPU profiling zones
(visible in Tracy or any `PROFILE_ZONE`-compatible profiler).

### CPU Zones (`PROFILE_ZONE` — Main Thread)

| Zone | Location | What it measures |
|------|----------|------------------|
| `NBody Physics` | `app.c` | Top-level wrapper (parent of all below) |
| `NBody Verlet` | `scene.c` | `nbody_step()` — O(N²) gravity + Velocity Verlet |
| `NBody Trail Sample` | `scene.c` | `trail_renderer_record()` — ring buffer writes |
| `NBody Instance Build` | `scene.c` | `nbody_write_instances()` — model matrix generation |
| `NBody VBO Upload` | `scene.c` | `instanced_group_update()` → `glBufferSubData` |
| `Trail Ribbon Build` | `trail_renderer.c` | `build_ribbon()` × N bodies — CPU geometry staging |
| `Trail VBO Upload` | `trail_renderer.c` | `glBufferSubData` — GPU buffer upload |
| `Trail Draw Calls` | `trail_renderer.c` | `glMultiDrawArrays` — batched N strips in 1 call |

### GPU Zones (`GPU_STAGE_PROFILER` — OpenGL Context)

| Zone | Location | What it measures |
|------|----------|------------------|
| `Instanced Render` | `scene.c` | Sphere draw call (shared with material grid) |
| `NBody Trails` | `scene.c` | Full trail rendering pass |

---

## Resource Lifecycle

When N-body mode is toggled **OFF**, the engine restores the original material
grid by calling `scene_init_instancing()` a second time.  This re-init path must
first release every resource allocated by the previous init call to avoid GPU
buffer and CPU memory leaks:

```c
/* Cleanup before re-init (scene.c — N-body OFF branch) */
trail_renderer_cleanup(&scene->trail_renderer);
#ifdef USE_TRANSPARENT_BILLBOARDS
if (scene->billboard_instances) {
    platform_aligned_free(scene->billboard_instances);
    scene->billboard_instances = NULL;
}
billboard_sorter_cleanup(&scene->billboard_sorter);
#endif
instanced_group_cleanup(&scene->instanced_group);
billboard_group_cleanup(&scene->billboard_group);
scene_init_instancing(scene);
```

The cleanup mirrors what `scene_cleanup()` does at application exit.  Without it,
each ON → OFF toggle leaked ~27 KB (4 blocks: instance VBO, billboard VBO,
aligned instance array, sphere sorter).

Validated with `just test-integration-valgrind-full` — **0 bytes definitely lost**.

---

## Code Map

| File | Role |
|------|------|
| `include/nbody.h` | Public API, constants, data structures |
| `src/nbody.c` | Physics implementation (Verlet, softening, preset) |
| `tests/test_nbody_stability.c` | Stability test suite (8 tests) |
| `include/trail_renderer.h` | Trail rendering (visual ribbons behind bodies) |
| `src/trail_renderer.c` | Billboard ribbon trail implementation |
| `src/app_input.c` | Simulation speed (`,` / `.`) and gravity (Shift variants) keybindings |
| `src/app_ui.c` | N-body HUD overlay (energy, gravity, stability) |
| `src/app_binding.c` | F2 help overlay registration |
| `include/shockwave.h` | Shockwave VFX API and data structures |
| `src/shockwave.c` | Shockwave renderer (init, emit, update, draw) |
| `shaders/shockwave.vert` | Billboard vertex shader (camera-facing quad) |
| `shaders/shockwave.frag` | Procedural expanding ring fragment shader |
| `shaders/trail.vert` | Trail vertex shader |
| `shaders/trail.frag` | Trail fragment shader |
