# IDE & Environment Setup Guide

This guide explains how to set up a robust development environment for **Suckless OGL**, focusing on modern C tooling (clangd, CMake) and compatibility with containerized workflows like **Distrobox**.

## 1. Prerequisites

### Essential Tools

- **CMake** (3.14+)

- **Clang / GCC** (C11 support)

- **Make** or **Ninja**

- **clangd** (Language Server)

### Recommended Editor

**VS Code** is highly recommended for its excellent `clangd` integration.

- Install the **clangd** extension (`llvm-vs-code-extensions.vscode-clangd`).

- _Optional_: **CodeLLDB** for debugging (`vadimcn.vscode-lldb`).

---

## 2. The Setup Architecture

This project uses a **hybrid build approach** to ensure headers are visible to both the build system (inside Distrobox/Docker) and your IDE (on the Host).

### Key Components

1. **CMake FetchContent**: We force `FetchContent` for third-party libraries (like GLFW) instead of using system packages (`pkg-config`).
   - _Why?_ This downloads sources into `build/_deps/`, ensuring headers are physically present in the project folder.

2. **Compilation Database**: We generate `compile_commands.json` which tells `clangd` exactly where to look for headers.

---

## 3. Configuration Steps

### Step 1: Generate Compilation Database

Run this command once (or whenever you add new CMake targets). It works inside Distrobox or on a native Debian/Linux system.

```bash

# Clean build (optional but recommended for fresh setup)

rm -rf build/

# Configure and generate compile_commands.json

# Note: We configure in 'build/' but link the JSON to root for clangd

cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Create symlink for clangd

ln -sf build/compile_commands.json compile_commands.json

```

**What this does:**

- Downloads GLFW, cglm, stb, etc. into `build/_deps/`.

- Generates `build/compile_commands.json` with absolute paths to those headers.

- Links it to the root so your editor finds it.

### Step 2: VS Code Configuration

If you haven't already, accept the workspace recommendation to install `clangd`.

1. Open the project in VS Code.

2. If prompted, **Disable IntelliSense** for the Microsoft C/C++ extension (to allow `clangd` to take over).

3. Open `src/main.c`. You should be able to Ctrl+Click on functions like `glfwInit` or `app_run`.

---

## 4. Troubleshooting

### Headers not found (red squiggles)

If `clangd` reports missing headers despite the build working:

1. **Check for `compile_commands.json`**: Ensure the symlink exists in the project root.

2. **Verify Include Paths**: Open `compile_commands.json` and search for `-I.../build/_deps/glfw-src/include`. If it's missing, re-run `cmake -B build`.

3. **Reload Window**: VS Code sometimes caches old paths. run `Ctrl+Shift+P` -> `Developer: Reload Window`.

4. **Rebuild**:

```bash
    # Sometimes generating the headers (glad) requires a build

    make
```

### Distrobox vs Host Paths

If you build inside a container but edit on the host:

- **The Fix**: Our `CMakeLists.txt` forces local dependency builds (`FetchContent`). This avoids the issue where `pkg-config` points to `/usr/include` inside the container (which doesn't exist on the host).

- **Validation**: Ensure you **DO NOT** use system-installed GLFW dev packages for the _build configuration_ if you want IDE completions to work perfectly on the host.

---

## 5. Debian / Native Linux Compatibility

This setup is fully compatible with Debian 13 or any other distro without Distrobox.

- **Requirement**: You need X11 development headers installed system-wide, as GLFW compiles from source.

```bash
    sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```
