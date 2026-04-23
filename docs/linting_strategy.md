# Linting Strategy and Caching

This document outlines the static analysis strategy for the `suckless-ogl` project and the implementation of its high-performance caching mechanism.

## Strategy: Clang-Tidy

We use `clang-tidy` for static analysis. The configuration is defined in `.clang-tidy`, focusing on:

- **Security**: Avoiding insecure buffer handling and deprecated APIs.
- **Reliability**: Detecting narrowing conversions and uninitialized variables.
- **Readability**: Enforcing consistent coding styles and removing "magic numbers".
- **Portability**: Ensuring compliance with C standards (CERT, HICPP).

### Style Preferences

We prioritize "Suckless" philosophy:

- Minimize external dependencies.
- **`NOLINT` comments are forbidden** — fix the root cause instead (see [No-Suppression Policy](#no-suppression-policy) below).
- Use `static const` or `enum` instead of magic numbers.

## Incremental Caching (Sentinel Files)

Originally, we explored `cltcache`. However, due to its overhead and specific requirement for explicit compiler flags (`--`), we transitioned to a native **Sentinel-based caching system** implemented directly in the `Makefile`.

### How it Works

Instead of linting every file on every run, we use "Sentinel files" (`.linted`) to track the status of each source file.

1. **Dependency Tracking**: Each `.linted` file in `.lint_cache/` depends on:
    - The corresponding `.c` source file.
    - The project's `.clang-tidy` configuration.
    - The `compile_commands.json` database.
2. **Date Comparison**: `make` natively compares the timestamp of the source vs. the sentinel. If the source is older than the sentinel, the file is skipped.
3. **Updating**: If a file needs linting, `clang-tidy` is executed. On success, the sentinel file is updated using `touch`.
4. **Dependencies**: Before linting, the system ensures that generated headers (like `glad/glad.h`) are ready by building the necessary targets.
5. **Parallelization**: The process is parallelized using `make -j$(NPROCS)`, allowing simultaneous analysis of multiple files.

### Why this approach?

- **Speed**: Subsequent runs are near-instantaneous (O(1) file stat check).
- **Robustness**: If an analysis is interrupted, the sentinel isn't updated, ensuring it runs again on the next try.
- **Simplicity**: No external Python dependencies or complex cache databases; it leverages the operating system's file system and standard build tools.
- **Visibility**: The `Makefile` output clearly shows which file is being processed, providing immediate feedback.

## Maintenance

To clear the cache and force a full re-lint:

```bash
make lint-clean
make lint
```

Training a new rule in `.clang-tidy` will also automatically invalidate the entire cache, ensuring project-wide compliance.

## Include Hygiene (misc-include-cleaner)

The `misc-include-cleaner` check is enabled in `.clang-tidy` to detect unused `#include` directives at lint time.

### Configuration

```yaml
# .clang-tidy (excerpt)
Checks: '...,misc-*,...'
CheckOptions:
  - key: misc-include-cleaner.MissingIncludes
    value: 'false'
```

- **UnusedIncludes**: Enabled — flags headers that are included but never directly used.
- **MissingIncludes**: Disabled — avoids false positives on symbols available through transitive includes (common with cglm, stb, GLFW).

This ensures `just lint` catches stale includes automatically, without requiring IDE-specific tooling.

## GLSL Shader Validation

Shaders are validated at lint time using `glslangValidator` via `scripts/lint_shaders.sh`.

### Standard Mode (integrated in `just lint`)

```bash
just lint
# Includes: clang-tidy + ruff + GLSL validation (26 shaders)
```

Validates all `.vert`, `.frag`, and `.comp` shaders in `shaders/`. The script resolves custom `@header` include directives before passing the resolved source to `glslangValidator`.

### Strict Mode (optional, SPIR-V target)

```bash
just lint-shaders-strict
```

Runs validation with `--target-env opengl` (SPIR-V rules). This surfaces issues like missing `layout(location=N)` qualifiers that cause RenderDoc's shader debugger to fail silently.

As of March 2026, **all 33 shader files pass strict SPIR-V validation**. The project enforces explicit `layout(location=N)` on all varyings and non-opaque uniforms, and `layout(binding=N)` on all samplers/images. See [renderdoc_guide.md](renderdoc_guide.md#8-shader-debugging-spir-v-compatibility) for the full rationale.

## No-Suppression Policy

**`NOLINT`, `NOLINTNEXTLINE`, and `NOLINTBEGIN`/`NOLINTEND` are forbidden.** The correct approach is always to fix the root cause.

An automated guard (`scripts/check_nolint.sh`) enforces this at every stage of the development workflow:

| Stage | Trigger | Mechanism | Blocking |
|-------|---------|-----------|----------|
| **Local commit** | `git commit` on `*.c` / `*.h` | pre-commit hook (`check-nolint` in `.pre-commit-config.yaml`) | Yes (bypass: `--no-verify`) |
| **Local manual** | `just check-nolint` or `make check-nolint` | Direct script invocation | Yes (exit 1) |
| **CI — push** | Push to any branch | `lint-and-format` job step | Yes — fails the job |
| **CI — pull request** | PR opened/updated targeting master | `lint-and-format` job step | Yes — fails the job |
| **CI — scheduled** | Nightly cron (01:00 UTC) | `lint-and-format` job step | Yes — fails the job |

### How it Works

The script compares `git diff <base_ref>...HEAD` on `*.c` and `*.h` files, searching for added lines (`+`) containing `NOLINT`. If any are found, the check fails with a listing of all violations.

```bash
# Local usage
just check-nolint                    # Compare vs origin/master (default)
just check-nolint origin/main        # Custom base ref
make check-nolint NOLINT_BASE_REF=origin/main
```

### Exception Policy

If suppression is the **only viable path**, it requires:

1. An explicit comment explaining why the fix is not possible.
2. A clear assessment confirming no alternative exists.
3. A tracking issue to revisit and remove the suppression.
4. **Explicit user validation** before committing (use `--no-verify`).
