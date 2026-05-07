# Cache Locality vs Decoupling: Memory Layout Analysis

*May 2026 — Phase 10: Architecture Deepening V*

## Context

During the Phase 10 architecture deepening, we audited the struct layout of the core types (`App`, `Scene`, `PostProcess`) to understand the trade-offs between opaque-pointer decoupling and CPU cache performance.

## Struct Size Inventory

| Type | Size | Cache Lines (64B) | Location |
|------|------|-------------------|----------|
| `App` | 534 KB | 8355 | Stack (main) |
| `Scene` | 133 KB | 2080 | Inline in App |
| `SceneVisuals` | 132 KB | 2067 | Inline in Scene |
| `TrailRenderer` | 128 KB | 2058 | Inline in SceneVisuals |
| `PostProcess` | 2.8 KB | 44 | Inline in App |
| `SceneLighting` | 488 B | 8 | Inline in Scene |
| `SceneConfig` | 44 B | 1 | Inline in Scene |
| `IcosphereGeometry` | 72 B | 1 | Inline in Scene |
| `InstancedGroup` | 12 B | <1 | Inline in Scene |
| `BillboardGroup` | 24 B | <1 | Inline in Scene |

## Memory Layout Problem

```
Scene (133 KB = 2080 cache lines)
 offset    field                size        access frequency
───────────────────────────────────────────────────────────────
 +0        geometry             72 B        10/frame (init-time mostly)
 +72       instanced_group      12 B        7/frame  (HOT: per-draw)
 +84       billboard_group      24 B        12/frame (HOT: per-draw)
 +112      billboard_sorter     96 B        4/frame
 +208      simulation*          8 B (ptr)   29/frame
 +216      gpu*                 8 B (ptr)   105/frame (HOT)
 +224      shaders*             8 B (ptr)   93/frame  (HOT)
 +232      ████ SceneVisuals ██████████████ 132 KB of COLD data ██████
 +132576   lighting             488 B       (HOT: per-draw uniforms)
 +133064   config               44 B        (HOT: per-frame checks)
───────────────────────────────────────────────────────────────
```

### The Problem

`SceneVisuals` (132 KB) sits between hot fields (`gpu*`, `shaders*` at +216/+224) and other hot fields (`lighting` at +132576, `config` at +133064). This creates a **2067 cache line gap** between frequently co-accessed data.

The L1 data cache is typically 32–48 KB. The cold `SceneVisuals` block alone exceeds L1 capacity, meaning accessing `scene->lighting` after `scene->shaders` will always be an L1 miss, even though both are accessed every frame in every draw call.

### Root Cause: TrailRenderer

```
SceneVisuals (132 KB)
├── Skybox             20 B    (accessed ~2/frame)
├── TrailRenderer      128 KB  (accessed ~3-5/frame, COLD)
│   └── rings[32]      32 × TrailRing
│       └── points[256] × vec3 + timestamps[256] × float = ~4 KB/ring
└── ShockwaveRenderer  552 B   (accessed ~1/frame)
```

`TrailRenderer` stores `32 × 256 × (vec3 + float) = 128 KB` of position history inline. This data is:
- Written to ~60 times/second (sample recording)
- Read once/frame for GPU upload (trail drawing)
- **Never** accessed in a tight CPU loop alongside rendering data

## Existing Opaque Pointers: Performance Impact Assessment

Three sub-structs were already moved to heap-allocated opaque pointers:

| Field | Size (estimated) | Accesses/frame | Indirection cost |
|-------|-----------------|----------------|-----------------|
| `scene->simulation*` | ~4 KB | 29 | ~120 ns/frame |
| `scene->gpu*` | ~2 KB | 105 | ~420 ns/frame |
| `scene->shaders*` | ~8 KB | 93 | ~372 ns/frame |

**Total indirection overhead: ~900 ns/frame** vs frame budget of **16,600,000 ns** (60 fps).

**Impact: 0.005% of frame budget** — completely negligible.

These are accessed per-draw-call (10–20 calls/frame), never per-vertex or per-pixel. The CPU hot path is purely orchestration; the real work happens on the GPU.

## When Opaque Pointers Are Safe

✅ **Safe to opacify** (negligible perf impact):
- Structs accessed < 200 times/frame from CPU
- Structs not accessed inside tight loops (per-particle, per-vertex)
- Structs > 1 KB that push hot neighbors out of cache

❌ **Do NOT opacify**:
- Tiny structs (< 64 bytes) already in the same cache line as hot data
- Data accessed in per-particle loops (`NBodyParticle bodies[]`)
- GPU buffer handles used in draw loops (keep inline for prefetch)

## Recommended Action: Heap-Allocate SceneVisuals

Moving `SceneVisuals` (including `TrailRenderer`) to a heap pointer brings `lighting` and `config` within 1–2 cache lines of `gpu*` and `shaders*`:

```
Scene AFTER (estimated ~900 bytes, 14 cache lines)
 offset    field                size
───────────────────────────────────────
 +0        geometry             72 B
 +72       instanced_group      12 B
 +84       billboard_group      24 B
 +112      billboard_sorter     96 B
 +208      simulation*          8 B (ptr)
 +216      gpu*                 8 B (ptr)
 +224      shaders*             8 B (ptr)
 +232      visuals*             8 B (ptr) ← was 132 KB inline
 +240      lighting             488 B
 +728      config               44 B
 +772      hdr_files            ...
───────────────────────────────────────
```

Now `shaders*` (+224) and `lighting` (+240) are **on the same cache line**. A single L1 fetch serves both.

## Other Safe Decoupling Opportunities

See the companion analysis below for opportunities that reduce compile-time coupling without any runtime cost.

### Forward Declarations (Zero-Cost)

Replace `#include` with `typedef struct X X;` when only pointers are used:

| Header | Current include | Can forward-declare |
|--------|----------------|---------------------|
| `scene.h` | `#include "app_settings.h"` | Yes (only uses `AAMode` enum — extract to tiny header) |
| `app.h` | `#include "postprocess_internal.h"` | No (PostProcess is by-value — but becomes Yes after opacification) |
| `app.h` | `#include "scene.h"` | No (Scene is by-value) |

### Interface Headers (Compile Firewall)

Create minimal "interface" headers that expose only function signatures + opaque types:

- `postprocess.h` already exists as the public API
- `scene_internal.h` already gates struct access
- **Missing**: `scene_visuals.h` currently exposes full struct → after opacification, only pointer needed

### Header Include Reduction in postprocess_internal.h

`postprocess_internal.h` pulls 16 headers into 12 consumers. After extracting FX structs into a `pp_effects_state.h` (included only by init/apply/cleanup), the "internal" header drops to ~8 includes.
