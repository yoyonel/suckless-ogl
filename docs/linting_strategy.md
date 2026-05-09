# Linting Strategy and Caching

This document outlines the static analysis and formatting strategy for the `suckless-ogl` project and the implementation of its high-performance caching mechanism.

## Coding Style Philosophy

This project follows a coding style heavily inspired by the **Linux kernel** ([`Documentation/process/coding-style.rst`](https://www.kernel.org/doc/html/latest/process/coding-style.html)), adapted for a C11 OpenGL rendering engine.

### Why the Linux Kernel Style?

The kernel style was designed for large-scale C codebases maintained by thousands of contributors over decades. Its rules optimize for **readability at scale**, **grep-ability**, and **minimal cognitive load** — the same qualities we value in this project.

Other notable C conventions we drew from:

| Convention | Origin | Key Takeaway |
|---|---|---|
| [**Linux kernel coding style**](https://www.kernel.org/doc/html/latest/process/coding-style.html) | Torvalds, kernel community | Primary inspiration: tabs-8, braces, 80 cols, `goto` cleanup, short functions |
| [**SEI CERT C**](https://wiki.sei.cmu.edu/confluence/display/c) | Carnegie Mellon SEI | Security-focused rules (MEM, ERR, STR) — enforced via `clang-tidy cert-*` checks |
| [**FreeBSD style(9)**](https://man.freebsd.org/cgi/man.cgi?query=style&sektion=9) | FreeBSD project | Close to kernel style; explicit about blank lines between functions, `return` without parens |
| [**SQLite**](https://www.sqlite.org/codeofethics.html) | D. Richard Hipp | 100% branch coverage, `goto` cleanup, self-contained C — a model for quality |
| [**id Software**](https://github.com/id-Software/Quake-III-Arena) (Quake/Doom) | John Carmack | Pragmatic C for real-time graphics; performance-aware naming and layout |

### Core Principles (from the Kernel)

1. **Indentation = 8 spaces (tabs)** — Forces short nesting; if you need 3+ levels, refactor (kernel §1)
2. **80-column limit** — Encourages splitting complex expressions and extracting helpers (kernel §2)
3. **Opening braces on same line** (except functions) — Linux/K&R brace style (kernel §3)
4. **One blank line between function definitions** — Visual separation; never stack definitions without breathing room
5. **Functions should be short** — Do one thing; fit on one or two screenfuls (kernel §6)
6. **`goto` for centralized cleanup** — Single exit path for error handling, not cascading `if/else` (kernel §7). See [goto cleanup Pattern](goto_cleanup_pattern.md)
7. **`snake_case` everywhere** — Functions, variables, struct members. `UPPER_CASE` for macros only (kernel §4)
8. **No typedefs on structs** — Exception: opaque handles and function pointers (kernel §5)

## Code Formatting (Clang-Format)

All C, header, and GLSL files are formatted with `clang-format`. The configuration lives in `.clang-format` at the repository root.

### Rule Reference

Each rule is mapped to its kernel style rationale:

| Option | Value | Kernel Reference | Effect |
|--------|-------|-----------------|--------|
| `BasedOnStyle` | `Google` | — | Base style (overridden below to match kernel conventions) |
| `IndentWidth` | `8` | §1: "Tabs are 8 characters" | 8-space logical indentation; discourages deep nesting |
| `UseTab` | `ForIndentation` | §1: "use tabs" | Tabs for indentation, spaces for alignment |
| `BreakBeforeBraces` | `Linux` | §3: "K&R brace placement" | Opening brace on same line; functions on next line |
| `PointerAlignment` | `Left` | §4: "declare close to the type" | `int* p` style (C convention) |
| `ColumnLimit` | `80` | §2: "the preferred limit is 80 columns" | Hard wrap at 80 columns |
| `AllowShortFunctionsOnASingleLine` | `None` | §6: functions should be readable | Never collapse function bodies to a single line |
| `SeparateDefinitionBlocks` | `Always` | §1, FreeBSD style(9) | **Enforce a blank line between every top-level definition** (functions, structs, enums) |
| `SortIncludes` | `true` | — | Alphabetical include ordering (GLAD first via `IncludeCategories` priority) |

### `SeparateDefinitionBlocks`

This rule (available since clang-format 14) ensures consistent visual separation between function definitions, struct declarations, and enum definitions. Without it, `clang-format` leaves inter-definition spacing untouched — adjacent function bodies with no blank line in between pass silently.

With `Always`, `clang-format` automatically inserts a blank line wherever one is missing, and removes double blank lines where they are excessive.

The Linux kernel enforces this by convention during code review. FreeBSD's `style(9)` states it explicitly: *"Use blank lines to separate logical sections of code, including between function definitions."* We automate it via clang-format.

### CI Enforcement

Formatting is enforced in CI by the `lint-and-format` job:

```yaml
# .github/workflows/main.yml (excerpt)
make format CONTAINER_RUN=""
git diff --exit-code || (echo "❌ Format error" && exit 1)
```

Any file that would be reformatted by `clang-format` causes the job to fail. Run `just format` locally before committing.

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

The script checks for new `NOLINT` additions in `*.c` and `*.h` files using a two-layer approach:

1. **Committed changes**: `git diff <base_ref>...HEAD` detects NOLINT in already-committed code.
2. **Staged changes**: `git diff --cached <base_ref>` detects NOLINT in content about to be committed (index). This is critical during `pre-commit`, where `HEAD` still points to the previous commit and staged content would otherwise be invisible.

When staged files exist, their final content is compared against the base ref, correctly handling cases where staged changes *add* or *remove* NOLINT suppressions. If any net new NOLINT lines are found, the check fails with a listing of all violations.

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
