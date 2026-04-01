# Using Just (Command Runner)

The project now supports [Just](https://github.com/casey/just) as a modern alternative to `make`. `Just` provides a command runner for project-specific tasks, offering cleaner syntax, better argument handling, and robust cross-platform capabilities.

## Installation

To use `just`, you need to install it. Checks [Just Installation Guide](https://github.com/casey/just#installation) for details.

On most Linux distributions:

```bash
# Ubuntu/Debian
sudo apt install just

# Fedora
sudo dnf install just

# Arch
sudo pacman -S just
```

## Quick Start

To see all available commands, simply run:

```bash
just
# or
just --list
```

This will output a formatted list of all recipes with descriptions.

## Common Commands

Here is a mapping of common `make` commands to their `just` equivalents:

| Task | Makefile | Justfile | Notes |
| :--- | :--- | :--- | :--- |
| **Build** | `make` | `just build` | Builds debug version by default |
| **Run** | `make run` | `just run` | Builds and runs the app |
| **Clean** | `make clean` | `just clean` | CMake clean |
| **Clean All** | `make clean-all` | `just clean-all` | Removes build directory completely |
| **Test All** | `make test` | `just test` | Runs all tests |
| **Test Specific** | `make test/<name>` | `just test <name>` | Runs matching tests |
| **ApiTrace Unit** | `make test-apitrace` | `just test-apitrace` | Automated performance check |
| **ApiTrace Integr** | `make test-integration-apitrace` | `just test-integration-apitrace` | Full app scenario check |
| **Integration (Simple)** | `make test-integration`* | `just test-integration` | Fast UI scenario check (Debug) |
| **Integration (Valgrind)** | `make test-integration` | `just test-integration-valgrind` | UI scenario check under Valgrind |
| **Coverage** | `make coverage` | `just coverage` | Generates HTML report |
| **Lint** | `make lint` | `just lint` | Runs clang-tidy and ruff |
| **Format** | `make format` | `just format` | Runs clang-format and ruff |
| **Docs** | `make docs` | `just docs` | Builds MkDocs + Doxygen |
| **Docker Build** | `make docker-build` | `just docker-build` | Multi-stage production build |
| **Docker Run** | `make docker-run` | `just docker-run` | Run with X11 forwarding |
| **Docker Prune** | `make docker-clean-all` | `just docker-clean-all` | Clean all containers/images/cache |
| **Windows Config** | `-` | `just configure-win` | Configure CMake for MinGW cross-compilation |
| **Windows Build** | `-` | `just build-win` | Builds the application for Windows (.exe) |
| **Windows Run** | `-` | `just run-win` | Runs the Windows application via Wine |
| **Windows Test** | `-` | `just test-win` | Runs integration tests via Wine |
| **Windows Unit Test** | `-` | `just test-win-unit` | Runs unit tests (CTest) via Wine |

## Advanced Usage

### Testing

Run a specific test interactively with real-time logs:

```bash
just test test_app
```

Run a group of tests matching a pattern:

```bash
just test shader
```

This command automatically:

1. Builds the test binary.
2. If binary exists, runs it directly (via `xvfb` wrapper) for immediate output.
3. If not found, falls back to `ctest` pattern matching.

### UI Integration Testing

The project uses `xdotool` and `Xvfb` to simulate user interactions and verify UI stability across different build modes.

- **Standard**: `just test-integration` (Debug) / `just test-integration-release` (Release)
- **Memory Safety**: `just test-integration-valgrind` / `just test-integration-asan`
- **Profiling**: `just test-integration-profile` / `just test-integration-tracy`
- **Variants**: `just test-integration-ssbo` / `just test-integration-ultra` / `just test-integration-small` / `just test-integration-sync`
- **Combined**: `just test-integration-tracy-asan` / `just test-integration-tracy-release`

> [!TIP]
> Use `just test-integration` for quick functional verification. It finishes in seconds, whereas Valgrind variants can take several minutes.

### Build Variants

- **Release**: `just release` / `just run-release`
- **Small**: `just small` / `just run-small` (MinSizeRel)
- **SSBO**: `just build-ssbo` / `just run-ssbo` (Shader Storage Buffer Objects)
- **Sync Debug**: `just build-sync` / `just run-sync` (Check for GL errors synchronously)
- **ASan**: `just asan` / `just memcheck-asan` (AddressSanitizer)

### Analysis & Tools

- **Coverage**: `just coverage` - Builds with instrumentation and generates a report in `build-coverage/coverage_report/index.html`.
- **Valgrind**: `just memcheck` - Runs the app under Valgrind.
- **Perf**: `just perf` - Runs Linux `perf` profiler (requires privileges).
- **ApiTrace Performance**:
  - `just test-apitrace`: Automated lightweight unit-test level check.
  - `just test-integration-apitrace`: Automated full application scenario check with user interaction.
  - `just trace-perf`: Legacy manual analysis of GPU trace.

### Dependency Management & Environment

The `Justfile` automatically detects your environment:

- **Distrobox**: If you have `distrobox` and the `clang-dev` container, commands run inside it automatically.
- **Python**: It detects `uv`, `.venv`, or system `python3` automatically (ISO Makefile logic).
- **Deps**: `just deps-setup` downloads offline dependencies using the script.

## Why Just?

- **Arguments**: Passing arguments is native (`just test pattern` vs `make test/pattern` or `make test-one TEST=pattern`).
- **Variables**: Logic for variables like python runtime is cleaner.
- **Shell**: Recipes are executed in `bash` by default, but can use others (e.g. `python`).
- **Discovery**: `just --list` provides a self-documenting interface.
