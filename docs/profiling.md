# Profiling with Tracy

This project integrates [Tracy Profiler](https://github.com/wolfpld/tracy) for real-time CPU and GPU profiling.

## Prerequisites

- CMake
- A C++ compiler (since Tracy Client is C++)
- Tracy Server (GUI)

## Building with Tracy

To enable Tracy profiling, configure the build with `-DENABLE_TRACY=ON`.

Using `Just`:

```bash
just configure-profile
just build-profile
```

Or manually:

```bash
cmake -B build -DENABLE_TRACY=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

## Running the Application

Run the application as usual. It will attempt to connect to the Tracy Server on `localhost:8086`.

```bash
just run-profile
```

## Analyzing the Profile

1.  Download or build the Tracy Server (GUI) matching version `v0.10` (or the version specified in `CMakeLists.txt`).
2.  Start the Tracy Server.
3.  Click "Connect" while the application is running.

## Integration Details

-   **C++ Bridge**: `src/tracy_ogl_bridge.cpp` and `include/tracy_ogl_bridge.h` provide a C-compatible interface to Tracy's C++ GPU profiling features.
-   **GPU Profiling**: The `GPU_STAGE_PROFILER` macro automatically adds Tracy GPU zones when `ENABLE_TRACY` is ON.
-   **CPU Profiling**: `TracyC.h` macros (like `TracyCZone` and `TracyCFrameMark`) are used in the main loop and render functions.
-   **Zero Overhead**: When `ENABLE_TRACY` is OFF, all Tracy code is compiled out, ensuring zero overhead and no C++ runtime dependency.
