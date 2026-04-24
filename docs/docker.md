# Docker Support

This project includes a containerized build and runtime environment for consistent, reproducible builds across different systems. The Docker setup uses a multi-stage architecture with build caching and headless rendering support.

## Prerequisites

- **Docker** or **Podman** installed (auto-detected by Makefile)
- BuildKit support enabled (default in modern Docker/Podman)

## Quick Start

### Build the Image

```bash
just docker-build
```

This creates an optimized container image with:

- Multi-stage build (builder + minimal runtime)
- CMake build cache persistence
- Xvfb for headless OpenGL rendering

### Run the Application

```bash
# Software rendering (Mesa llvmpipe)
just docker-run

# Hardware-accelerated rendering (Intel/AMD GPU)
just docker-run-gpu
```

Runs the application in a container with X11 forwarding to your host display.

## Architecture

### Multi-Stage Build

The [`Dockerfile`](https://github.com/yoyonel/suckless-ogl/blob/main/Dockerfile) uses two stages:

#### Stage 1: Builder (fedora:41)

- Full development toolchain (clang, cmake, ninja, git)
- **BuildKit cache mount** on `/src/build` for incremental builds
- Compiles in Release mode with parallel build
- Copies only the final binary to `/tmp/app`

```dockerfile
RUN --mount=type=cache,target=/src/build \
    cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTS=OFF \
    && cmake --build build --parallel \
    && cp build/app /tmp/app
```

> [!TIP]
> The cache mount persists CMake's build directory between builds, dramatically speeding up rebuilds by reusing compiled object files and fetched dependencies.

#### Stage 2: Runtime (fedora:41)

- Minimal runtime dependencies only:
  - `glfw` - Window and input handling
  - `mesa-libGL`, `mesa-libEGL`, `mesa-dri-drivers` - OpenGL software rendering
  - `mesa-vulkan-drivers` - Vulkan/DRI support for GPU passthrough
  - `xorg-x11-server-Xvfb` - Virtual framebuffer for headless rendering
  - `gamemode` - Runtime library for Performance Mode
- Non-root user (`appuser`) for security
- Contains only: binary, assets, shaders, entrypoint script

**Image size comparison:**

- Builder stage: ~1.2 GB (with full toolchain)
- Final runtime image: ~400 MB (minimal dependencies)

### Headless Rendering with Xvfb

The [`entrypoint.sh`](https://github.com/yoyonel/suckless-ogl/blob/main/entrypoint.sh) script manages virtual display:

```sh
#!/bin/bash
set -e

# Start Xvfb in background
Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
XVFB_PID=$!

# Wait for Xvfb to start
sleep 2

# Set DISPLAY
export DISPLAY=:99

# Run the application
./app

# Cleanup
kill $XVFB_PID 2>/dev/null || true
```

This enables:

- **CI/CD testing** without physical display
- **Headless rendering** for automated screenshots/validation
- **Consistent environment** across different systems

### Build Context Optimization

The `.dockerignore` file aggressively excludes unnecessary files from the build context.
Only the essential source files, shaders, assets, and CMake configuration are sent to Docker:

```text
build/          # Build artifacts
build-*/        # Coverage/debug builds
_deps/          # CMake FetchContent cache (rebuilt in container)
deps/           # Pre-built dependencies
.git/           # Git history
docs/           # Documentation
site/           # MkDocs output
tests/          # Test suite (disabled in container via -DBUILD_TESTS=OFF)
Testing/        # CTest artifacts
*.md            # Markdown files
*.profraw       # LLVM profiling data
*.profdata      # LLVM coverage data
```

This reduces context transfer from **~870 MB to ~2 MB**, dramatically speeding up `docker build`.

!!! note "Tests disabled in container"
    The container build uses `-DBUILD_TESTS=OFF` since the `tests/` directory is excluded
    from the build context. This is intentional — unit tests run natively via `just test-all`.

## Justfile Targets

### Build & Run Targets

| Target | Description | `just` command |
|--------|-------------|----------------|
| Build Image | Build image with layer caching | `just docker-build` |
| Build No Cache | Force full rebuild | `just docker-build-no-cache` |
| Run (Software) | Run with X11 forwarding (Mesa llvmpipe) | `just docker-run` |
| Run (GPU) | Run with host GPU passthrough (Intel/AMD) | `just docker-run-gpu` |

### Maintenance Targets

| Target | Description | `just` command |
|--------|-------------|----------------|
| Clean Dangling | Remove dangling images | `just docker-clean` |
| Clean All | Prune all images and cache | `just docker-clean-all` |
| Disk Usage | Show disk usage stats | `just docker-usage` |

## GPU Passthrough

By default, `just docker-run` uses **software rendering** (Mesa llvmpipe) via Xvfb.
For hardware-accelerated rendering, use `just docker-run-gpu` which passes the host GPU
through to the container via the DRI (Direct Rendering Infrastructure).

### Requirements

- Linux host with Intel or AMD GPU
- `/dev/dri` device directory accessible
- User in the `video` group on the host
- X11 display available (`$DISPLAY` set)

### How It Works

```bash
just docker-run-gpu
```

This mounts the host's GPU device and adds the `video` group:

```bash
docker run --rm -it \
    --device /dev/dri \
    --group-add video \
    ...
    suckless-ogl:latest /bin/bash -c "export DISPLAY=$DISPLAY && ./app"
```

### Performance Comparison

| Mode | Renderer | IBL BRDF LUT Time |
|------|----------|-------------------|
| Software (`docker-run`) | llvmpipe (Mesa) | ~1000 ms |
| GPU (`docker-run-gpu`) | Mesa Intel Iris Xe | ~88 ms |

!!! warning "NVIDIA GPUs"
    NVIDIA GPUs require the [NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/overview.html)
    and `--gpus all` instead of `--device /dev/dri`. This is not yet supported by the project.

## Advanced Usage

### Custom Container Engine

The Makefile auto-detects Docker or Podman:

```makefile
CONTAINER_ENGINE := $(shell command -v docker 2> /dev/null || echo podman)
```

To force a specific engine:

```sh
CONTAINER_ENGINE=podman make docker-build
```

### Incremental Builds

Thanks to BuildKit cache mounts, subsequent builds are fast:

```sh
# First build: ~2-3 minutes (fetch deps, compile everything)
make docker-build

# Modify src/shader.c
# Second build: ~10-20 seconds (recompile only changed files)
make docker-build
```

### Running with Host X11 (Software Rendering)

The `docker-run` target forwards X11 and uses software rendering:

```bash
just docker-run
# Equivalent to:
docker run --rm -it \
    --cap-add=SYS_NICE \
    --ulimit rtprio=99 \
    --security-opt label=disable \
    --network host \
    -e DISPLAY=$DISPLAY \
    -e DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus" \
    -v /run/user/$(id -u)/bus:/run/user/$(id -u)/bus \
    -v /var/lib/dbus/machine-id:/var/lib/dbus/machine-id:ro \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    suckless-ogl /bin/bash -c "export DISPLAY=$DISPLAY && ./app"
```

For hardware-accelerated rendering with the host GPU, see [GPU Passthrough](#gpu-passthrough) above.

### GameMode & Real-Time Priority

To enable **Performance Mode** (SCHED_FIFO) and GameMode inside the container, specific permissions are required:

- `--cap-add=SYS_NICE`: Allows the container to set real-time scheduling policies.
- `--ulimit rtprio=99`: Allows the non-root `appuser` to request real-time priority.
- **D-Bus Mounting**: Essential for `libgamemode` to communicate with the host's GameMode daemon.

```

> [!WARNING]
> X11 forwarding requires `xhost +local:` which temporarily disables access control. This is handled automatically by the Makefile.

## Troubleshooting

### Build Cache Not Working

Ensure BuildKit is enabled:

```sh
# Docker
export DOCKER_BUILDKIT=1

# Podman (enabled by default)
```

### Xvfb Fails to Start

Check if port :99 is available:

```sh
docker run --rm -it suckless-ogl /bin/bash
# Inside container:
Xvfb :99 -screen 0 1920x1080x24
```

### X11 Permission Denied

Reset xhost permissions:

```sh
xhost +local:
make docker-run
```

## CI/CD Integration

Example GitHub Actions workflow:

```yaml
- name: Build Docker Image
  run: make docker-build

- name: Run Tests in Container
  run: |
    docker run --rm suckless-ogl /bin/bash -c "
      export DISPLAY=:99 &&
      Xvfb :99 &
      sleep 2 &&
      ./app --test
    "
```
