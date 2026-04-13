---
description: "Use when running tests, validating refactoring, checking code quality, or completing any task. Covers CI validation, test coverage, code quality standards, and documentation gates."
applyTo: ["src/**/*.c", "include/**/*.h", "tests/**/*.c", "shaders/**", "Justfile", "CMakeLists.txt", "scripts/**"]
---
# Testing & Quality Gates

## CI IS THE SINGLE SOURCE OF TRUTH (NO EXCEPTIONS)

**A task (prompt, refactoring, fix, feature, docs) is NEVER complete until the CI pipeline is fully green.**
This is an absolute, non-negotiable rule with ZERO exceptions:
- All jobs must pass: `lint-and-format`, `test-and-coverage`, AND `build-production`
- A red CI means the task is still **in progress**, regardless of local test results
- Do NOT merge, do NOT close issues, do NOT mark tasks as done with red CI
- If CI fails after merge to master, a hotfix PR is mandatory — master must be green at all times

## Validation Checklist (every change)

Run locally before pushing:

```bash
just format         # Auto-fix formatting (clang-format)
just lint           # clang-tidy + shellcheck + yamllint + hadolint (0 warnings)
just test-all       # All unit + integration tests pass (CTest)
just ci-docker-all  # Full local CI matrix (Docker, mimics GitHub Actions)
```

After push, monitor CI: check GitHub Actions workflow status.

## Test Coverage

- **Never decrease test coverage.** Every new function in `src/` must have corresponding tests.
- When refactoring: run `just test-all` before AND after — same test count, same results.
- When adding a feature: add unit tests in the same PR. No "tests in a follow-up" pattern.
- Track coverage: `just coverage-llvm` (preferred) or `just coverage` (gcovr).
- **Target**: >78% lines, >85% functions, >50% branches.

## Code Quality Standards

- Zero `clang-tidy` warnings (`-Werror` equivalent — enforced by CI and hooks)
- Zero `clang-format` diff (enforced by CI)
- No warning suppression (`NOLINT`, `NOLINTNEXTLINE`) — fix root cause instead
- `shellcheck` clean for all scripts in `scripts/`
- `hadolint` clean for `Dockerfile` and `Dockerfile.ci`
- `yamllint` clean for all YAML files

### No Suppression Policy

**NEVER bypass linting or compiler warnings using suppression mechanisms:**

❌ Forbidden patterns:
- `// NOLINT`, `// NOLINTNEXTLINE`, `/* clang-tidy-disable */`
- `ignore:` lists in `.hadolint.yaml`, `pylintrc`, etc.
- `continue-on-error: true` in GitHub Actions to hide failures
- CMake `set_source_files_properties(... SKIP_LINTING ON)`

✅ Correct approach: fix the root cause.

**Exception policy**: If suppression is the **only viable path**, it requires:
1. An explicit comment in the code explaining why the fix is not possible
2. A clear assessment confirming no alternative exists
3. A tracking note (issue) to revisit and remove the suppression
4. User explicit validation before committing

## CI/CD Validation

### Local CI (Docker, mimic GitHub Actions)

```bash
just ci-docker-all
```

This runs: static checks, lint, build + test (Release/Debug), memory checks (ASan/UBSan), coverage report.

### Remote CI (GitHub Actions)

- Automatically triggered on `push` + `pull_request` to `master`
- Jobs: `lint-and-format`, `test-and-coverage`, `test-windows`, `documentation`, `build-production`, `build-production-windows`
- Artifacts: coverage HTML, test frames

### Failing Builds = Blocked

- If any CI job fails (locally or remotely) → Fix before moving forward
- No workarounds or skipping checks

## Justfile Discipline

- Recipes are thin entry points (max ~5 lines of shell)
- Complex logic goes to `scripts/*.sh`
- New scripts must be executable (`chmod +x`) and have a usage comment header
- Test new Justfile recipes locally before pushing

## Documentation Gate (blocks PR merge)

A PR is NOT ready for merge unless ALL applicable documentation is included:

1. **Feature docs** — New features must have `docs/<feature>.md` + `docs/<feature>.fr.md`
2. **mkdocs.yml** — Updated `nav:` section if new doc added
3. **Keybinding sync** — `src/app_binding.c` updated if controls changed
4. **No local filesystem links** in any documentation
5. **All code blocks tagged** with language (`` ```bash ``, `` ```c ``, `` ```glsl ``, etc.)
6. **Mermaid diagrams** for architecture/flow changes
7. **Multilingual sync** — Both `.md` and `.fr.md` must exist and be consistent

## RenderDoc Observability (Mandatory for rendering changes)

For every feature, fix, or refactor touching rendering or GPU resources:

- Add meaningful resource names via GL debug utilities for new objects
- Add meaningful labels around logical GPU blocks (passes, phases)
- Prefer hierarchical labels for Event Browser navigation
- Use stable naming conventions (`Feature_ObjectType[_Index]`)
- Update `docs/tracing.md` when changing labels or naming conventions
