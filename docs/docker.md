# Docker Support

This project includes a containerized build and runtime environment for consistent, reproducible builds across different systems. The Docker setup uses a multi-stage architecture with build caching and headless rendering support.

## Prerequisites

- **Docker** or **Podman** installed (auto-detected by Makefile)

- BuildKit support enabled (default in modern Docker/Podman)

## Quick Start

### Build the Image

```sh
make docker-build

```

This creates an optimized container image with:

- Multi-stage build (builder + minimal runtime)

- CMake build cache persistence

- Xvfb for headless OpenGL rendering

### Run the Application

```sh
make docker-run

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
    && cmake --build build --parallel \
    && cp build/app /tmp/app

```

> [!TIP]
> The cache mount persists CMake's build directory between builds, dramatically speeding up rebuilds by reusing compiled object files and fetched dependencies.

#### Stage 2: Runtime (fedora:41)

- Minimal runtime dependencies only:
  - `glfw` - Window and input handling

  - `mesa-*` - OpenGL drivers

  - `mesa-*` - OpenGL drivers

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

### Build Cache Optimization

The `.dockerignore` file excludes unnecessary files from the build context:

```text
build/          # Build artifacts

build-*/        # Coverage/debug builds

.git/           # Git history

docs/           # Documentation

*.md            # Markdown files

```

This reduces context size and speeds up `docker build`.

## Makefile Targets

### Build Targets

| Target         | Description                    | `make`                  | `just`                  |
| -------------- | ------------------------------ | ----------------------- | ----------------------- |
| Build Image    | Build image with layer caching | `docker-build`          | `docker-build`          |
| Build No Cache | Force full rebuild             | `docker-build-no-cache` | `docker-build-no-cache` |
| Run App        | Run with X11 forwarding        | `docker-run`            | `docker-run`            |

### Maintenance Targets

| Target         | Description                | `make`             | `just`             |
| -------------- | -------------------------- | ------------------ | ------------------ |
| Clean Dangling | Remove dangling images     | `docker-clean`     | `docker-clean`     |
| Clean All      | Prune all images and cache | `docker-clean-all` | `docker-clean-all` |
| Disk Usage     | Show disk usage stats      | `docker-usage`     | `docker-usage`     |

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

### Running with Host X11

The `docker-run` target forwards X11:

```sh
make docker-run

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

### GameMode & Real-Time Priority

To enable **Performance Mode** (SCHED_FIFO) and GameMode inside the container, specific permissions are required:

- `--cap-add=SYS_NICE`: Allows the container to set real-time scheduling policies.

- `--ulimit rtprio=99`: Allows the non-root `appuser` to request real-time priority.

- **D-Bus Mounting**: Essential for `libgamemode` to communicate with the host's GameMode daemon.

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

    docker run --rm suckless-ogl /bin/bash -c "
    export DISPLAY=:99 &&
    Xvfb :99 &
    sleep 2 &&
    ./app --test
    "
```
