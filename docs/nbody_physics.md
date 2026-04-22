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
clamp accumulator to NBODY_MAX_ACCUMULATOR (1/30 s)
while accumulator >= NBODY_FIXED_DT:
    integrate_step(NBODY_FIXED_DT)
    accumulator -= NBODY_FIXED_DT
```

The max-accumulator clamp prevents a spiral of death after lag spikes or on
the first frame.

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

## Test Validation

The automated test suite (`tests/test_nbody_stability.c`) verifies physics
invariants over long simulation runs:

| Test | What it checks | Threshold |
|------|---------------|-----------|
| `test_nbody_single_step_sanity` | One step does not explode | Positions finite |
| `test_nbody_energy_conservation` | Energy bounded after init boost | $\Delta E / E_0 < 5\%$ |
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
| `NBODY_MAX_ACCUMULATOR` | 1/30 s | Max accumulated physics time |

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
2. **Stability:** `Stability: Stable (drift: 0.12%)` — shows whether the
   integrator is conserving energy.

The stability indicator is colour-coded:

| Colour | State | Condition |
|--------|-------|-----------|
| Green | Stable | drift $< 5\%$ |
| Red | Divergent | drift $\geq 5\%$ |

Drift is computed as:

$$\text{drift} = \frac{|E(t) - E_0|}{|E_0|}$$

where $E_0$ is the total energy snapshot taken at initialisation (or after
a gravity change).

### Trail Length Stability

The trail sampling accumulator scales by `time_scale`:

```text
effective_dt = wall_dt × time_scale
sample_timer += effective_dt
while sample_timer >= TRAIL_SAMPLE_INTERVAL:
    record positions
    sample_timer -= TRAIL_SAMPLE_INTERVAL
```

This ensures the trail ring buffer fills and evicts points at the same
*visual* pace regardless of `time_scale`. At 8× speed, 8 samples are emitted
per frame instead of 1, keeping trail length constant in world units.

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
| `Trail Draw Calls` | `trail_renderer.c` | `glDrawArrays` × N bodies |

### GPU Zones (`GPU_STAGE_PROFILER` — OpenGL Context)

| Zone | Location | What it measures |
|------|----------|------------------|
| `Instanced Render` | `scene.c` | Sphere draw call (shared with material grid) |
| `NBody Trails` | `scene.c` | Full trail rendering pass |

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
| `shaders/trail.vert` | Trail vertex shader |
| `shaders/trail.frag` | Trail fragment shader |
