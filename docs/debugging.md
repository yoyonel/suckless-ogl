# Debugging Guide

This guide explains the debugging tools and features available in the project, specifically focusing on the OpenGL Debug Output system.

## OpenGL Debug Output

To maximize performance on integrated GPUs like Intel Iris Xe (where CPU-GPU sync points can cause thermal/power throttling), the engine uses a highly optimized, asynchronous debugging model:

- **Conditional Debug Context:** The OpenGL debug context (`GLFW_OPENGL_DEBUG_CONTEXT`) is only requested in Debug builds (`#ifndef NDEBUG`). In Release mode, driver validation is disabled to allow fast-path execution.
- **Zero-Overhead Debug Groups:** Debug annotations (`gl_debug_push_group` / `gl_debug_pop_group`) are compiled as `static inline` empty functions in Release mode, completely removing the function call and driver overhead.
- **Asynchronous Error Handling:** All runtime calls to the synchronous `glGetError()` have been eliminated, avoiding pipeline stalls. Errors are instead reported asynchronously via the `glDebugMessageCallback` in Debug builds.

### High Sensitivity Mode (Debug Builds)

When the debug context is active in Debug builds:

- Synchronous output (`GL_DEBUG_OUTPUT_SYNCHRONOUS`) is enabled so that the callback is executed immediately when the driver generates a message, allowing for accurate stack traces in a debugger.
- A deduplication mechanism prevents the log from being flooded with identical messages (e.g., inside a draw loop).
- Messages of severity `GL_DEBUG_SEVERITY_NOTIFICATION` (e.g., verbose resource allocations or layout details) are filtered out via `glDebugMessageControl` to avoid callback overhead.

### Log Level Mapping

When the callback is active, the system maps OpenGL message severities to application log levels as follows:

| OpenGL Severity | OpenGL Type | Application Log Level | Description |
| :--- | :--- | :--- | :--- |
| `HIGH` | Any | **ERROR** | Critical errors that likely caused undefined behavior or crashes. |
| Any | `ERROR` | **ERROR** | Any message explicitly flagged as an error type. |
| `MEDIUM` | Any | **WARNING** | Major performance warnings, deprecated behavior, or undefined behavior usage. |
| `LOW` | Any | **WARNING** | Minor performance warnings or redundancy. |

### Interpretation

- **[ERROR]**: Needs immediate attention.
- **[WARNING]**: Should be investigated. Often points to non-optimal usage (e.g., "Buffer object usage hint is STATIC_DRAW but is being updated frequently").
- **[INFO]**: Generally safe to ignore unless you are debugging a specific resource issue (e.g., "Texture object 4 created").

## Performance Debugging with ApiTrace

The engine is integrated with **ApiTrace** to detect GPU-side performance bottlenecks and driver-level stalls.

### Automated Checks

Performance verification is automated and integrated into the build system:

- **`just test-apitrace`**: Runs the unit test suite under ApiTrace and fails if any "performance issue" or "stall" warnings are detected in the command stream.
- **`just test-integration-apitrace`**: Launches the main application, executes a full user scenario (Page Up/Down, F-keys, camera move), and analyzes the resulting trace for regressions.

### Interpreting Stalls

The automation looks for specific driver warnings, such as:
> `api performance issue 1: memory mapping a busy "buffer" BO stalled and took 1.379 ms.`

If such an error occurs, it means the CPU is waiting for the GPU before it can continue, which kills the framerate. Fixes usually involve double-buffering resources or using `GLsync` fences (see [GPU Synchronization Guide](./gpu-rendering-synchronization.md)).

### Manual Investigation

If the automated tests fail, you can investigate the trace manually:

1. `make qapitrace`: Launches the ApiTrace GUI to inspect the offending frame and command.
2. `make replay`: Replays the trace to verify visual consistency.
