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
- Avoid `NOLINT` comments unless absolutely necessary (e.g., global variables for test context).
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
4. **Parallelization**: The process is parallelized using `make -j$(NPROCS)`, allowing simultaneous analysis of multiple files.

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
