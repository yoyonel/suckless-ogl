# Analysis Report: Memory Management and Shallow Copies

## 1. Audit Context

Following the fix in commit `9abba91c`, a thorough analysis of the codebase was performed to identify similar risks of double-free memory corruption caused by shallow copies of structures managing heap-allocated resources.

### The Identified Problem

In `gpu_profiler_ui.c`, the function `gpu_profiler_ui_compact_stages` performed a value assignment (`*dest = *src`) when compacting profiling stages. The `GPUStage` struct contains two `AdaptiveSampler` instances, each owning a `samples` pointer to a dynamically allocated buffer.

A shallow copy of `GPUStage` caused ownership of these buffers to be shared. During final cleanup via `gpu_profiler_cleanup`, the function attempted to free the same pointer multiple times, causing a crash.

## 2. Fix Analysis

The fix introduces `gpu_stage_move`, which:

1. Copies scalar fields one by one.
2. **Swaps** the buffer pointers of the samplers instead of copying them.

This approach guarantees unique ownership for each allocated buffer while reusing existing memory slots in the `stages` array.

## 3. Codebase Audit Results

The audit covered the following structures identified as managing dynamic memory:

| Structure | Critical Resource | Risk Status | Conclusion |
|-----------|-------------------|-------------|------------|
| `GPUStage` | `AdaptiveSampler.samples` | **Fixed** | Risk contained by `gpu_stage_move`. |
| `AdaptiveSampler` | `samples` (data buffer) | Low | Initialized by `init`, freed by `cleanup`. No copy detected. |
| `AsyncRequest` | `float_data`, `half_data` | Low | Uses a safe destructive ownership-transfer pattern in `async_loader_poll`. |
| `PBRMaterial` | `name` (fixed array) | None | POD (Plain Old Data) struct. Safe to copy. |
| `MaterialLib` | `materials` (dynamic array) | None | Managed exclusively via pointers (`MaterialLib*`). No value copy. |
| `SphereInstance` | None (Vectors/Matrices) | None | POD struct. Safe to copy (used heavily in `sphere_sorting.c`). |

### Specifically Examined Points

* **`sphere_sorting.c`**: Performs `SphereInstance` copies during sorting. This is entirely safe as `SphereInstance` contains only value types (cglm math types).
* **`async_loader.c`**: Passing `AsyncRequest` from the worker thread to the main thread is safe. The loader explicitly nulls its internal pointers after copying to the requester, preventing any ownership conflict.

## 4. Recommendations and Best Practices

1. **Prefer Fixed Arrays for Strings**: As seen with `PBRMaterial` and `GPUStage`, using `char name[N]` instead of `char* name` eliminates risks related to struct copying.
2. **Move Semantics in C**: For complex structures (like `GPUStage`), always implement a "move" or "swap" function instead of using the `=` operator.
3. **Ownership Documentation**: Explicitly document whether a structure "owns" its pointers or merely references them.
4. **Use Static Analysis**: Continue using `just lint-full` (clang-tidy) which helps detect use-after-free issues.

## 5. Evaluation Conclusion

The double-free risk from shallow copies of samplers was isolated to the profiler UI compaction logic. The consistent use of pointers and the prevalence of POD structs in the rest of the engine guarantee good overall memory management stability.
