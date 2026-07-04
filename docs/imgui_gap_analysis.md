# ImGui Gap Analysis: suckless-odin vs suckless-ogl

> **Sources analysed**
> - Odin reference: `src/gui/gui.odin` (1317 l.), `gui_compute.odin` (383 l.), `gui_postfx.odin` (1388 l.)
> - C++ port: `src/gui.cpp` (1323 l.), `src/gui.h`
> - Reference screenshots from suckless-odin runtime

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Fully implemented and functional |
| ⚠️ | Partially implemented or stub/placeholder |
| ❌ | Absent from suckless-ogl |
| 🔴 | Critical gap (core functionality broken or mocked) |
| 🟠 | Important gap (significant missing feature) |
| 🟡 | Quality gap (DX, ergonomics, discoverability) |

---

## 1. Global / Cross-Cutting Features

### 1.1 Tooltips `(?)`

Every slider, checkbox, and combo in suckless-odin has a `(?)` help label that, on hover, shows
a `SetTooltip` with: what the parameter does, its units, and practical advice.

**suckless-ogl status**: Tooltips exist on ~5 controls out of ~80. The rest are bare labels.

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| Camera sliders — all 8 params have `(?)` | ✅ | ❌ | 🟠 |
| Scene controls — tooltips | ✅ | ❌ (2/10) | 🟠 |
| Rendering controls — tooltips | ✅ | ❌ | 🟠 |
| Post-FX per-effect tooltips | ✅ | ❌ | 🟠 |
| MBlur tab — all params annotated | ✅ | ❌ | 🟠 |
| Compute Tuning — SPBRDF, SPMap, IRMap tooltips | ✅ | ❌ | 🟠 |
| Compute Tuning — slicing params tooltips | ✅ | ❌ | 🟠 |

### 1.2 Reset Buttons

suckless-odin provides per-parameter `Reset` or `Reset to Defaults` buttons calling
`pipeline_reset_effect(p, effect)` or overwriting with the compile-time default value.
In suckless-ogl **no reset mechanism exists anywhere** in the GUI.

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| Post-FX: per-effect `Reset` in Settings tree | ✅ | ❌ | 🟠 |
| Compute Tuning: per-slider `Reset` button | ✅ | ❌ | 🟠 |
| MBlur tab: `Reset to Defaults` button | ✅ | ❌ | 🟠 |
| Camera tab: `Reset Camera` button | ✅ | ✅ | OK |

### 1.3 A/B Split per Post-FX Effect

Each Post-FX effect in Odin exposes: an **A/B Split checkbox**, a **position slider** (`← %.0f%% →`)
and a **colored `[S]` badge** using the Glasbey palette for multi-split legibility.
suckless-ogl has **no per-effect A/B split** whatsoever.

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| Per-effect A/B split + position slider | ✅ all effects | ❌ | 🔴 |
| Multi-split colored `[S]` indicator | ✅ | ❌ | 🟡 |
| Global A/B Split for Specular AA | ✅ | ❌ | 🟠 |

### 1.4 ImGui Docking

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| `ImGuiConfigFlags_DockingEnable` | ✅ | ❌ | 🟡 |
| Window can be detached/repositioned | ✅ | ❌ | 🟡 |

---

## 2. Camera Tab

**Parameters: parity. Missing: all tooltips on the C++ side.**

| Parameter | Odin | C++ | Tooltip |
|-----------|:----:|:---:|:-------:|
| Position display | ✅ | ✅ | — |
| Yaw / Pitch display | ✅ | ✅ | — |
| Speed | ✅ | ✅ | Odin ✅ C++ ❌ |
| Acceleration | ✅ | ✅ | Odin ✅ C++ ❌ |
| Friction | ✅ | ✅ | Odin ✅ C++ ❌ |
| Sensitivity | ✅ | ✅ | Odin ✅ C++ ❌ |
| Rotation Smoothing | ✅ | ✅ | Odin ✅ C++ ❌ |
| Mouse Smoothing | ✅ | ✅ | Odin ✅ C++ ❌ |
| FOV | ✅ | ✅ | Odin ✅ C++ ❌ |
| Head Bobbing toggle | ✅ | ✅ | Odin ✅ C++ ❌ |
| Bobbing Frequency | ✅ | ✅ | Odin ✅ C++ ❌ |
| Bobbing Amplitude | ✅ | ✅ | Odin ✅ C++ ❌ |
| Reset Camera button | ✅ | ✅ | — |

---

## 3. Scene Tab

| Parameter | Odin | C++ | Notes |
|-----------|:----:|:---:|-------|
| Skybox toggle | ✅ | ✅ | |
| Skybox Blur (LOD slider) | ✅ | ✅ | |
| Subdivisions (icosphere) | ❌ | ✅ | C++ exclusive |
| **Skybox Mode** (Equirect / Cubemap) | ✅ | ❌ | 🟠 runtime switch |
| **Blur Source** (Mipmap / IBL Prefilter) | ✅ | ❌ | 🟠 runtime switch |
| **Cubemap Mipmap Mode** (seamless) | ✅ | ❌ | 🟠 |
| **Show Blur Diff + Diff Gain** | ✅ | ❌ | 🟡 debug |
| PBR Debug Mode combo (10 modes, live) | ❌ placeholder | ✅ | C++ more complete |
| **GI Mode** (OFF / Volume / SSBO, live) | ❌ placeholder | ✅ | C++ exclusive |
| **Show Probe Grid** | ❌ | ✅ | C++ exclusive |
| Sort Mode | ✅ 3 modes | ✅ 3+GPU Bitonic | C++ has GPU Bitonic |
| Wireframe | ✅ | ✅ | |

---

## 4. Rendering Tab

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| **Edge AA checkbox + debug heatmap** | ✅ | ❌ | 🟠 |
| `(?)` tooltip on Edge AA | ✅ | ❌ | 🟡 |
| MSAA info (read-only) | ⚠️ | ✅ text only | |
| Specular AA toggle | ✅ | ✅ | |
| Specular AA Mode (Screen-Space / Curvature) | ✅ | ✅ | |
| `(?)` tooltip on Specular AA | ✅ | ❌ | 🟡 |
| **Specular AA debug views** (variance / diff) | ✅ | ❌ | 🟠 |
| **A/B Split — Specular AA** + position slider | ✅ | ❌ | 🟠 |
| Debug Views section (Fog, Histogram, Stencil) | ⚠️ placeholder | ❌ | |
| Profiling section (GPU Timeline, Metrics, Perf) | ⚠️ placeholder | ❌ | |
| Scene Debug section (N-Body, Probes, N-Body sliders) | ⚠️ placeholder | ❌ | |
| Environment section (HDR Index, Hot-Reload) | ⚠️ placeholder | ❌ | |

---

## 5. Post-FX Tab

### 5.1 Global Post-FX Controls

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| Enable Post-FX master toggle | ✅ | ✅ | |
| Preset combo (Default, Cinematic, …) | ✅ | ✅ (13 presets) | |
| Save / Load collapsible section | ✅ | ⚠️ stub | 🟠 |

### 5.2 Per-Effect Controls — Pattern

Every effect in Odin follows a pattern **entirely absent in C++**:

```
[✓] Effect Name  [S]           ← coloured A/B indicator
    ▶ Settings
        [Reset]                ← per-effect reset to defaults
        Slider (?)             ← tooltip on every parameter
        [✓] A/B Split          ← per-effect split-screen compare
            ← 50% →           ← split position slider
```

### 5.3 Per-Effect Feature Matrix

| Effect | Odin Reset | C++ Reset | Odin A/B | C++ A/B | Odin tooltips | C++ tooltips |
|--------|:----------:|:---------:|:--------:|:-------:|:-------------:|:------------:|
| Exposure | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Tonemapping | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Vignette | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Film Grain | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Chrom. Aberration | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Color Grading | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Bloom | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| FXAA | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Auto-Exposure | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Depth of Field | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Motion Blur | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Banding | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| Fog | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |
| LUT3D | ✅ | ❌ | ✅ | ❌ | ✅ | ❌ |

---

## 6. MBlur Tab (Dedicated)

suckless-odin has a **dedicated tab** with advanced motion blur controls.

| Feature | Odin (MBlur tab) | C++ (in Post-FX only) |
|---------|:-:|:-:|
| Enable Motion Blur | ✅ | ✅ |
| Intensity slider + `(?)` | ✅ | ❌ tooltip |
| Max Velocity slider + `(?)` | ✅ | ❌ tooltip |
| Samples slider + `(?)` | ✅ | ❌ tooltip |
| **Reset to Defaults** button | ✅ | ❌ |
| **Synthetic velocity injection** section | ✅ | ❌ |
| — Enable Injection checkbox | ✅ | ❌ |
| — Direction slider (0–360°) | ✅ | ❌ |
| — Magnitude slider (UV fraction) | ✅ | ❌ |
| — Quick presets: Right / Up / Left / Down | ✅ | ❌ |
| — Speed presets: Slow / Medium / Fast / Max | ✅ | ❌ |
| — Live UV vector readout | ✅ | ❌ |
| Debug visualization section | ✅ | ⚠️ basic checkbox |
| — Debug Mode combo: Velocity / Tile-Max / Neighbor-Max / Heatmap | ✅ 4 modes | ❌ |
| — Vector Field Debug toggle | ✅ | ❌ |
| State diagnostic (bit-flag readout) | ✅ | ❌ |

---

## 7. Profiling Tab — Critical Gaps

### 7.1 Performance Mode Widget

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| **Performance Mode section** at top | ✅ | ❌ | 🔴 |
| `(?)` tooltip (GameMode / SCHED_FIFO / nice) | ✅ | ❌ | |
| Enable/Disable checkbox | ✅ | ❌ | |
| Backend label `[GameMode]` / `[SCHED_FIFO]` / `[Nice]` | ✅ | ❌ | |
| Mesa restart notification | ✅ | ❌ | |

### 7.2 GPU Timer Table

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| **Enable Profiling** toggle | ✅ | ❌ | 🔴 |
| Table columns: Pass, **Avg**, **Min**, **Max**, % | ✅ 5 cols | ⚠️ 3 cols (Stage, ms, %) | 🟠 |
| **Min / Max** columns (statistical range, greyed) | ✅ | ❌ | 🟠 |
| **Total row** (Avg/Min/Max aggregates) | ✅ | ❌ | 🟠 |
| **Frame budget line** `PostFX: X.X% (Y.YY ms)` with green/yellow/red | ✅ | ❌ | 🟠 |

!!! note "Rolling statistics vs instant sample"
    The Odin table exposes rolling **min/max/avg** over a window — essential
    for spotting GPU spikes. C++ shows only the last frame's instantaneous sample.

---

## 8. Shaders Tab — Critical Gaps

### 8.1 Enable / Disable

| Feature | Odin | C++ |
|---------|:----:|:---:|
| Enable Variants checkbox | ✅ | ✅ |
| "Disabled — using uber-shader" status text | ✅ | ❌ |

### 8.2 Active Effects Display

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| **Active Effects** — indented bullet list with human-readable names | ✅ | ⚠️ hex flags only | 🟠 |

### 8.3 Cache Status

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| **Status: CACHED (program N)** / **MISS** coloured line | ✅ | ⚠️ static counter | 🟠 |

### 8.4 Actions

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| **Compile Current** button (calls `pipeline_compile_variant`) | ✅ | ❌ | 🔴 |
| Button disabled when already cached | ✅ | ❌ | 🟡 |
| "(already cached)" / "(cache full)" hint | ✅ | ❌ | 🟡 |
| **Clear All** button (destroys cache) | ✅ | ❌ | 🔴 |

### 8.5 Cached Variants Table

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| `Cached Variants: N / MAX` live counter | ✅ | ⚠️ static "0 or 1" | 🔴 |
| Table: #, Program ID, Effects (human-readable names) | ✅ | ❌ | 🔴 |
| Active variant highlighted in green | ✅ | ❌ | 🟡 |

---

## 9. Compute Tuning Tab

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| SPBRDF Sample Count + `(?)` | ✅ | ✅ no tooltip | 🟡 |
| SPMap Sample Count + `(?)` | ✅ | ✅ no tooltip | 🟡 |
| IRMap Sample Delta + `(?)` | ✅ | ✅ no tooltip | 🟡 |
| **Per-slider `Reset` button** | ✅ | ❌ | 🟠 |
| Mip 0/1/2 Slices + `(?)` | ✅ | ✅ no tooltip | 🟡 |
| Irradiance Slices + `(?)` | ✅ | ✅ no tooltip | 🟡 |
| **Mip Grouping Start Mip + `(?)`** | ✅ | ❌ | 🟠 |
| **Seamless Progressive Mip Threshold + `(?)`** | ✅ | ❌ | 🟠 |
| **Apply & Recalculate** (real callback wired) | ✅ real | ⚠️ mock | 🔴 |
| **Active Profile combo** | ✅ | ❌ | 🔴 |
| **Save Draft to profile** button | ✅ | ❌ | 🔴 |
| **Delete Profile** (with confirmation modal) | ✅ | ❌ | 🔴 |
| **Create new profile** (name input + Create) | ✅ | ❌ | 🔴 |
| **JSON persistence** (read / write config file) | ✅ | ❌ | 🔴 |
| Validation before apply / save | ✅ | ❌ | 🟠 |
| Status / error messages with auto-dismiss timer | ✅ | ⚠️ status only | 🟡 |

---

## 10. IBL Debug Tab

Near-parity on this tab.

| Feature | Odin | C++ |
|---------|:----:|:---:|
| Preview Size slider | ✅ | ✅ |
| Preview Exposure (EV) + right-click reset | ✅ | ✅ |
| Env Map: collapsible + pixel inspector | ✅ | ✅ |
| Irradiance Map: collapsible + inspector | ✅ | ✅ |
| Prefilter Map + Mip Roughness slider | ✅ | ✅ |
| BRDF LUT: collapsible + inspector | ✅ | ✅ |
| GPU Memory Estimate section | ✅ | ✅ |
| Scroll-to-section "Go To" from search | ✅ | ✅ |

---

## 11. UI / Application Configuration & Persistence

| Feature | Odin | C++ | Gap |
|---------|:----:|:---:|-----|
| Compute tuning profiles (JSON I/O) | ✅ | ❌ | 🔴 |
| Post-FX preset save / load (JSON) | ✅ | ⚠️ apply only | 🟠 |
| UI state persistence across sessions | ❌ | ❌ | — |
| Tab restore after search (`restore_tab`) | ✅ | ✅ | OK |

---

## 12. Porting Priority Roadmap

| Priority | Feature | Effort |
|----------|---------|--------|
| 🔴 1 | Compute Tuning — real Apply callback | Low |
| 🔴 2 | Compute Tuning — CRUD + JSON | Medium |
| 🔴 3 | Shaders — Compile Current + Clear All | Low |
| 🔴 4 | Shaders — Cached Variants live table | Low |
| 🔴 5 | Shaders — Active Effects bullet list | Low |
| 🟠 6 | Post-FX — per-effect Reset buttons | Low |
| 🟠 7 | Post-FX — per-effect A/B Split + position slider | Medium |
| 🟠 8 | Profiling — Enable Profiling toggle | Low |
| 🟠 9 | Profiling — Min/Max columns + Total row | Low |
| 🟠 10 | Profiling — Frame budget summary line | Low |
| 🟠 11 | Performance Mode widget | Medium |
| 🟠 12 | MBlur — dedicated tab with injection | Medium |
| 🟠 13 | Rendering — Edge AA + debug heatmap | Low |
| 🟠 14 | Rendering — Specular AA A/B Split | Low |
| 🟡 15 | Tooltips `(?)` on all parameters | Low (high volume) |
| 🟡 16 | ImGui Docking | Trivial (1 flag) |
| 🟡 17 | A/B split `[S]` indicator | Low |

---

## 13. Summary Scorecard

```
Feature domain           suckless-odin    suckless-ogl (C++)
──────────────────────────────────────────────────────────────
Tooltips (?)             ████████████     ██░░░░░░░░░░  ~17%
Reset buttons            ████████████     ░░░░░░░░░░░░   0%
A/B Split per effect     ████████████     ░░░░░░░░░░░░   0%
Post-FX controls         ████████████     ████████░░░░  ~65%
Profiling observability  ████████████     ████░░░░░░░░  ~35%
Shaders tab              ████████████     ████░░░░░░░░  ~35%
Compute Tuning           ████████████     ████░░░░░░░░  ~35%
MBlur dedicated tab      ████████████     ░░░░░░░░░░░░   0%
Performance Mode         ████████████     ░░░░░░░░░░░░   0%
IBL Debug                ████████████     ████████████  ~95%
Camera controls          ████████████     ████████░░░░  ~80%
──────────────────────────────────────────────────────────────
Overall estimate         100%             ~45%
```

!!! warning "Apply & Recalculate is a mock in suckless-ogl"
    The "Apply & Recalculate Active Environment" button in `src/gui.cpp`
    only sets a status message string. It does **not** call any compute
    shader recalculation. The Odin version calls `apply_compute_tuning`
    which recompiles and re-runs the IBL convolution pipeline.

!!! note "C++ exclusive features not yet ported to Odin"
    - GPU Bitonic Sort (parallel GPU sort mode)
    - `Subdivisions` icosphere mesh detail
    - `Show Probe Grid` toggle
    - `GI Mode` live selector (OFF / Volume 3D / SSBO)
    - `PBR Debug Mode` live combo (10 modes)
