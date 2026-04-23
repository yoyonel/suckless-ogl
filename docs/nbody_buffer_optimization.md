# N-Body Buffer Upload Optimization

## Problem Statement

During N-body animation (`Shift+G`), the application suffers from:

1. **High CPU usage** on the main thread
2. **GPU under-utilization** — the GPU idles while the CPU waits for synchronization

The root cause is **implicit CPU↔GPU synchronization** caused by
`glBufferSubData` calls on buffers still in use by the GPU from the
previous frame.

## Background: OpenGL Buffer Synchronization

When `glBufferSubData` is called on a buffer that the GPU is still reading
(from a draw call submitted earlier), the OpenGL driver has two options:

1. **Block the CPU** until the GPU finishes reading (implicit sync / stall)
2. **Allocate a new backing store** and let the GPU finish reading the old one

Option 1 is the default behavior for `glBufferSubData` — it creates a
**pipeline bubble** where neither the CPU nor the GPU is productive.

### Buffer Orphaning

**Buffer orphaning** is a well-known technique where the application calls:

```c
glBufferData(target, size, NULL, usage);   // orphan: allocate new store
glBufferSubData(target, 0, size, data);    // write to the new store
```

The `glBufferData(..., NULL, ...)` call tells the driver: "I don't need the
old data anymore." The driver can then:

- Let the GPU keep reading the old backing store
- Immediately give the CPU a new (or recycled) backing store to write to
- **No synchronization needed** — both CPU and GPU work in parallel

This is documented in the
[OpenGL Wiki — Buffer Object Streaming](https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming)
and is the standard approach for dynamic buffer updates.

## Affected Code Paths

### 1. Instance VBO (`instanced_group_update`)

**File**: `src/instanced_rendering.c`

Called once per frame from `scene_nbody_update()` to upload the 14
`SphereInstance` structs (model matrices, PBR materials, prev_position)
after physics integration.

**Before** (synchronous):

```c
void instanced_group_update(InstancedGroup* group, const SphereInstance* data,
                            int count)
{
    group->instance_count = count;
    glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(count * sizeof(SphereInstance)), data);
}
```

**After** (with orphaning):

```c
void instanced_group_update(InstancedGroup* group, const SphereInstance* data,
                            int count)
{
    group->instance_count = count;
    GLsizeiptr size = (GLsizeiptr)(count * sizeof(SphereInstance));
    glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}
```

**Data size**: 14 instances × 128 bytes = **1,792 bytes** per frame.

### 2. Trail VBO (`trail_renderer_draw`)

**File**: `src/trail_renderer.c`

Called once per frame during scene rendering to upload camera-facing
ribbon geometry built on the CPU. The staging buffer is rebuilt every
frame because the camera moves.

**Before** (synchronous):

```c
glBindBuffer(GL_ARRAY_BUFFER, trail->vbo);
glBufferSubData(GL_ARRAY_BUFFER, 0,
                (GLsizeiptr)(total_verts * sizeof(TrailVertex)),
                staging);
```

**After** (with orphaning):

```c
glBindBuffer(GL_ARRAY_BUFFER, trail->vbo);
glBufferData(GL_ARRAY_BUFFER,
             (GLsizeiptr)(MAX_TRAIL_VERTICES * sizeof(TrailVertex)),
             NULL, GL_STREAM_DRAW);
glBufferSubData(GL_ARRAY_BUFFER, 0,
                (GLsizeiptr)(total_verts * sizeof(TrailVertex)),
                staging);
```

**Data size**: up to 32 bodies × 514 vertices × 32 bytes = **~527 KB**
worst case (typically much less with 14 bodies and partial trails).

## Existing Pattern

The `billboard_group_update()` function in `src/billboard_rendering.c`
already uses this exact orphaning pattern:

```c
glBufferData(GL_ARRAY_BUFFER,
             (GLsizeiptr)(group->capacity * sizeof(SphereInstance)),
             NULL, GL_DYNAMIC_DRAW);
glBufferSubData(GL_ARRAY_BUFFER, 0,
                (GLsizeiptr)(count * sizeof(SphereInstance)), data);
```

This optimization aligns the instance and trail buffers with the
existing billboard buffer strategy.

## Programmatic Measurement

A benchmark test (`tests/test_benchmark_buffer_upload.c`) measures the
per-frame upload cost using the real application API. It exercises the
actual `instanced_group_update` and `trail_renderer_draw` code paths with
a full NBody simulation, measures wall-clock timing with
`clock_gettime(CLOCK_MONOTONIC)`, and validates data integrity via GPU
readback.

For full details on the benchmark architecture, parameters, baseline
results, and usage instructions, see the dedicated documentation:
[NBody Buffer Upload Benchmark](nbody_benchmark.md).

## Expected Impact

| Metric | Before | After (expected) |
|--------|--------|-------------------|
| Instance VBO upload | Blocked until GPU done | Immediate (orphan + write) |
| Trail VBO upload | Blocked until GPU done | Immediate (orphan + write) |
| GPU utilization | Drops during CPU upload stalls | Sustained — no bubbles |
| CPU frame time | Includes implicit sync wait | Pure compute + memcpy only |

The improvement is most visible when:

- The GPU workload is non-trivial (post-processing, IBL, high subdiv)
- Frame rate is high (vsync off → more frames → more stalls/second)
- Trail buffer is large (many bodies, long trails)

## Future Work

| Optimization | Description | Complexity |
|-------------|-------------|------------|
| Double-buffered VBOs | Ping-pong between 2 VBOs per frame | Medium |
| `glMapBufferRange` persistent | Map once, write every frame with fences | Medium |
| Compute shader ribbons | Build trail geometry on GPU, skip CPU staging entirely | High |

## References

- [OpenGL Wiki — Buffer Object Streaming](https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming)
- [NVIDIA — OpenGL Performance Guide](https://developer.nvidia.com/opengl-performance)
- [docs/gpu-rendering-synchronization.md](gpu-rendering-synchronization.md) — Project GPU sync docs
- [docs/nbody_physics.md](nbody_physics.md) — N-Body physics reference
- [docs/profiling_tracy.md](profiling_tracy.md) — Tracy profiling zones
