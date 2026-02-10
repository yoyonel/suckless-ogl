# Suckless-OGL

<div align="center">
  <p>
    <a href="https://github.com/yoyonel/suckless-ogl/actions"><img src="https://github.com/yoyonel/suckless-ogl/actions/workflows/main.yml/badge.svg" alt="CI/CD Pipeline"></a>
    <a href="https://yoyonel.github.io/suckless-ogl/"><img src="https://img.shields.io/badge/coverage-report-brightgreen" alt="Coverage Report"></a>
    <a href="https://github.com/yoyonel/suckless-ogl/releases"><img src="https://img.shields.io/github/v/release/yoyonel/suckless-ogl?include_prereleases&label=release&color=blue" alt="Latest Release"></a>
    <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
  </p>
  <h3>High-performance C11 PBR Renderer</h3>
</div>

![Suckless-OGL Reference Image](reference_image.png)

**Suckless-OGL** is a minimalist 3D rendering engine written in C. Faithful to the "suckless" philosophy, it prioritizes a compact codebase, rigorous resource management, and an absence of unnecessary dependencies. It implements a modern pipeline based on **OpenGL 4.4 Core Profile**.

## 🚀 Features

- **Minimalism**: Lightweight architecture focused on performance and readability.
- **Modern Rendering**: Support for Skyboxes, IcoSpheres, textures, and Phong lighting.
- **Dynamic Shaders**: Loading and compilation of GLSL files (vertex/fragment).
- **Shader Optimization**: Static (Release) or dynamic (Debug) compilation to balance flexibility and performance. [See Documentation](shader_optimization.md).
- **Stencil Buffer Masking**: Post-processing optimization to differentiate skybox from objects. [See Documentation](stencil_masking.md).
- **Performance Mode**: Adaptive optimization (GameMode/Native) for maximum frame stability. [See Documentation](perf_and_notifications.md).
- **Isolated Environment**: Native `distrobox` support to guarantee a reproducible build environment.
- **Quality & Testing**: Unit testing suite, code coverage, static analysis, and [Standalone Mocking](standalone_testing_mocking.md).

## 🛠️ Compilation and Usage

The project uses a `Makefile` wrapper that drives `CMake` to simplify interactions.
For a modern alternative, seeing [Justfile Documentation](justfile.md).

### Compilation Flags & Environment

The build is configured with the following settings:

- **Optimization**: `-Wall -Wextra -O2` for clean and performant code.
- **POSIX Standard**: `-D_POSIX_C_SOURCE=199309L` for `clock_gettime` support.
- **Static Analysis**: `clang-tidy` integration with strict header filters.
- **Containerization**: Default use of `distrobox` with the `clang-dev` image to isolate dependencies.

### Main Commands

| Command | Action |
| :--- | :--- |
| `make all` | Compiles the project (generates GLAD and the `app` binary). |
| `make debug` | Compiles in DEBUG mode (dynamic shaders, ideal for dev). |
| `make release` | Compiles in RELEASE mode (statically optimized shaders). |
| `make run` | Launches the DEBUG version. |
| `make run-release` | Launches the RELEASE version. |
| `make test` | Runs the unit test suite via `ctest`. |
| `make test/name` | Runs a single test (e.g. `make test/test_stencil_masking`). |
| `make test-list` | Lists all available test names. |
| `make format` | Applies `clang-format` formatting on `src`, `include`, and `tests`. |
| `make lint` | Runs `clang-tidy` static analysis on source files. |
| `make coverage` | Generates a complete HTML report via `llvm-cov` in `build-coverage/`. |

## 🤖 CI/CD Workflow (GitHub Actions)

The pipeline is structured to optimize the build while guaranteeing maximum quality. It handles Testing, Quality Assurance, Documentation, and Automated Releases.

**[> Read the full CI/CD Pipeline Documentation](cicd_pipeline.md)**

### Quick Summary

1. **Test & Coverage**: Instrumented compilation and test execution under **Xvfb**.
2. **Lint & Format**: Enforces styling (`clang-format`) and static analysis (`clang-tidy`).
3. **Automated Releases**:
   - **Nightly**: Built every night at 01:00 UTC and on every push to master.
   - **Stable**: Triggered by version tags (`v*`).

## 📁 Project Structure

- `src/` & `include/`: Engine core (Log, App, Shader, Texture, Icosphere).
- `shaders/`: GLSL sources (Phong, Background/Skybox).
- `assets/`: HDR resources and textures.
- `tests/`: Unit tests (Icosphere, Shader, Skybox, Texture, Log).
- `docs/`: In-depth technical documentation.

## 📦 Docker / Podman

To test the application in a container with X11 forwarding:

```sh
make docker-build
make docker-run
```

(Requires a local X server and configured xhost permissions).

📄 License

This project is licensed under the MIT License. See the LICENSE file for more details.
