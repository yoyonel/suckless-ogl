# Build System & Optimization Documentation

This document explains the available build targets and the debugging process used to resolve the SIMD alignment issues in the `release` build.

## Build Targets

The project uses a hybrid Makefile + CMake system for convenience and performance.

| Target | Build Type | Flags | Description |
| :--- | :--- | :--- | :--- |
| `make` | Debug | `-O0 -g` | Default dev build. Full symbols, no optimization. |
| `make release` | Release | `-O3 -march=native -ffast-math` | **Ultra Release**. Uses **Unity Build** (compile as single unit). Stripped. |
| `make debug-release`| RelWithDebInfo | `-O3 -march=native -g` | Release speed + Debug symbols. Not stripped. Use this for debugging performance issues or crashes. |
| `make small` | MinSizeRel | `-Os` | Focused on binary size (~192K). |

## The SIMD Segfault Story: A Technical Deep-Dive

When enabling `-march=native` with `-O3`, the application initially suffered from intermittent segfaults. Here is how we diagnosed and fixed it.

### 1. Diagnosis: Localization and Clues
The debugging followed a three-step deduction process:

*   **Localization (The "Last Words")**: By adding granular `LOG_INFO` traces, we observed the crash happened immediately after "Starting app_init_instancing" but before any further logs. This narrowed the "crime scene" to the first few lines of that function.
*   **The `-march=native` Clue**: The crash *only* occurred when the compiler was allowed to use native CPU instructions. This strongly suggested that the compiler was generating specialized high-speed instructions (AVX/AVX2) that the standard build wasn't using.
*   **Valgrind's Testimony**: Running `valgrind --leak-check=full ./build-release/app` reported `Conditional jump or move depends on uninitialised value(s)` during matrix operations. In the context of highly optimized code, this often indicates that a SIMD instruction (like `VMOVAPS`) tried to access an address it couldn't handle.

### 2. Root Cause: Alignment Violation
High-performance SIMD instructions (Single Instruction Multiple Data) like those enabled by AVX process 32 or 64 bytes at a time. To do this safely and at full speed, the hardware requires the memory address to be a **multiple of the data size** (aligned).

The problem was twofold:
1.  **Stack Alignment**: The `App` struct was initially on the stack. Most compilers only guarantee 16-byte alignment for the stack, but AVX needs 32 or 64.
2.  **Heap Alignment**: Standard `malloc` only guarantees 8 or 16-byte alignment. When we allocated arrays of structs, if the first element wasn't aligned to 64 bytes, every vectorized write to it caused an immediate hardware exception (Segfault).

### 3. The Fix: Enforced Alignment
We resolved this by ensuring the CPU always finds its data where it expects:

*   **Aligned Allocation**: Replaced stack and standard `malloc` allocation with `posix_memalign((void**)&ptr, SIMD_ALIGNMENT, size)`. This forces the operating system to give us a memory block starting at a perfectly aligned address.
*   **Struct Padding**: Added `__attribute__((aligned(SIMD_ALIGNMENT)))` to `SphereInstance` and `PBRMaterial`. This tells the compiler to pad these structures so that their size is always a multiple of 64, ensuring that elements in an array (`data[1]`, `data[2]`, etc.) stay aligned.
*   **Consistency**: Centralized `SIMD_ALIGNMENT 64` in `gl_common.h`. 64 bytes is chosen as it is both AVX-512 safe and matches the L1 cache line size of most modern CPUs, providing a secondary performance benefit by preventing "cache line splitting".
