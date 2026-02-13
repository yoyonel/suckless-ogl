# Profiling with Tracy

This project integrates [Tracy Profiler](https://github.com/wolfpld/tracy) for real-time CPU and GPU profiling.

## Prerequisites

- CMake
- A C++ compiler
- Tracy Server (GUI)

## Building the Tracy Server (GUI)

The Tracy Server is required to visualize the profiling data. You can build it directly from this repository:

1.  **Install Dependencies** (Debian/Ubuntu):
    ```bash
    sudo apt-get install libcapstone-dev libfreetype-dev libglfw3-dev
    ```

2.  **Build the Server**:
    ```bash
    just tracy-server
    ```

    This will clone Tracy (v0.10) into `tools/tracy` and compile the profiler GUI. The executable will be located at:
    `tools/tracy/profiler/build/unix/Tracy-release`

3.  **Run the Server**:
    ```bash
    ./tools/tracy/profiler/build/unix/Tracy-release
    ```

## Building the Application with Profiling

To enable Tracy profiling in the application:

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

1.  Start the Tracy Server (as described above).
2.  Run the application:
    ```bash
    just run-profile
    ```
3.  In the Tracy Server window, click "Connect".

## Integration Details

-   **C++ Bridge**: `src/tracy_ogl_bridge.cpp` and `include/tracy_ogl_bridge.h` provide a C-compatible interface to Tracy's C++ GPU profiling features.
-   **GPU Profiling**: The `GPU_STAGE_PROFILER` macro automatically adds Tracy GPU zones when `ENABLE_TRACY` is ON.
-   **CPU Profiling**: `TracyC.h` macros (like `TracyCZone` and `TracyCFrameMark`) are used in the main loop and render functions.
-   **Zero Overhead**: When `ENABLE_TRACY` is OFF, all Tracy code is compiled out, ensuring zero overhead and no C++ runtime dependency.
