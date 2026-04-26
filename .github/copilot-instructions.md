# suckless-ogl Development Guidelines

## Project Context

Suckless OpenGL rendering engine in C — PBR, IBL, compute shaders, post-processing pipeline.
Repository: [yoyonel/suckless-ogl](https://github.com/yoyonel/suckless-ogl).

## Roadmap & Project Tracking

**CRITICAL**: At the start of every session, consult the GitHub project board and milestones:
- Project board: https://github.com/users/yoyonel/projects/3
- Milestones: `gh api repos/yoyonel/suckless-ogl/milestones?state=all`
- Open issues: `gh issue list --state open`
- Refer to [docs/github-settings.md](../../docs/github-settings.md) for label taxonomy and project structure.

## Architecture Overview

| Subsystem | Key Files |
|-----------|-----------|
| **Core App** | `app.c`, `main.c`, `cli.c` |
| **Rendering** | `renderer.c`, `sphere_types.h`, `instanced_rendering.c`, `ssbo_rendering.c`, `billboard_rendering.c` |
| **PBR / IBL** | `pbr.c`, `material.c`, `ibl_coordinator.c`, `light_probes.c`, `skybox.c` |
| **Post-Processing** | `postprocess.c`, `effects/fx_*.c` (bloom, DoF, auto-exposure, FXAA, LUT, motion blur) |
| **Shaders** | 55+ GLSL files in `shaders/` (vertex, fragment, compute) |
| **Input** | `app_input.c`, `camera_input.c`, `postprocess_input.c`, `app_binding.c` |
| **Profiling** | `gpu_profiler.c`, `perf_timer.c`, `tracy_manager.c`, `effect_benchmark.c` |
| **Tests** | 59 test files in `tests/` (Unity framework, visual regression, benchmarks) |

## Build & Test

```bash
just build          # Debug build (Clang)
just test-all       # All unit + integration tests (CTest)
just lint           # clang-tidy + shellcheck + yamllint + hadolint (0 warnings)
just format         # clang-format (C/GLSL)
just coverage-llvm  # LLVM-Cov coverage report
just ci-docker-all  # Full local CI matrix (Docker)
```

## 🚫 Golden Rules

1. **NEVER push to `origin/master`** — Not in any case, under any circumstance
2. **Format + Lint + Tests REQUIRED** — All must pass before commit
3. **Docs always in sync** — Update or create docs in `docs/` and keep `mkdocs.yml` updated with every feature/fix
4. **Pre-commit checks** — Enforce via `just pre-commit-install`
5. **CI/CD validation** — All builds/tests must pass locally (Docker) AND remote (GitHub Actions)
6. **NO suppression of warnings/errors** — Fix issues at the source; never bypass them
7. **NEVER modify reference test images** — Files matching `tests/references/ref_*.png` are the visual regression baseline from `master`. They must NEVER be replaced, overwritten, or regenerated without the user's **explicit approval and visual validation**. When in doubt, restore them from `origin/master` with `git checkout origin/master -- tests/references/ref_*.png`
8. **MVP first** — Always start with a Minimum Viable change on a limited scope to validate the approach before scaling to the full codebase. Do NOT batch-modify all files upfront; prove the fix on one representative case, get user validation, then generalize
9. **Impact assessment before implementation** — Before starting any task that involves architectural changes, GPU pipeline modifications, or significant refactoring, FIRST produce a written analysis covering: (a) expected performance impact (gain/regression, with reasoning), (b) risks and unknowns, (c) resource cost (tokens, time, complexity). For GPU/rendering work, this means predicting whether compute vs raster will win based on the workload profile (bandwidth-bound? ALU-bound? occupancy?). Present the analysis to the user for go/no-go BEFORE writing any code. A 5-minute analysis can save hours of wasted implementation

## Detailed Rules (see instruction files)

The following instruction files contain the authoritative, detailed rules. **Do not duplicate their content here.**

- **commit-workflow.instructions.md** — SoC commit discipline, Conventional Commits format, pre-commit workflow, branching strategy, git safety rules.
- **testing-quality.instructions.md** — CI validation, test coverage, code quality standards, no-suppression policy, Justfile discipline, documentation gates, RenderDoc observability.
- **github-project.instructions.md** — PR workflow, labels, milestones, project board management, issue lifecycle.

---

## 📚 Documentation Strategy

- **Multilingual Synchronization**: All documentation must be kept synchronized across all available languages (currently **English** and **French**). When updating or creating a doc in `docs/` (`.md`), ensure the equivalent `.fr.md` is also updated or created.

### Testing Requirements

- **Feature Coverage**: Every new feature or significant modification must be covered by appropriate tests.
  - **Unit Tests**: For core logic, parsers, and parameter validation.
  - **Integration Tests**: For rendering presets, UBO synchronization, and full pipeline verification.
- **Regression Testing**: Ensure all existing tests pass after modifications.

### Update Existing Docs
- Feature/fix updates existing behavior → Update the relevant doc
- Examples: `docs/tooling.md`, `docs/ci_cd.md`, `docs/runtime_controls_logging.md`
- Always verify section structure after edit

### Create New Docs
- New feature/module/subsystem → Create `docs/<feature>.md`
- Update `mkdocs.yml` `nav:` section to include new doc
- Use clear hierarchy: H1 (feature name), H2 (subtopics), H3 (details)

### UI & Interaction Synchronization

1. **Keyboard Overlay (F2)**: Always keep the in-app keyboard help synchronized.
   - When adding, removing, or changing a keybinding in `src/app_input.c` or elsewhere, you **MUST** update the `AppBindingRegistry` in `src/app_binding.c`.
   - The `F2` help system is the source of truth for the user; code-only changes are considered incomplete.

### Documentation Best Practices

1. **No local filesystem links** — All docs assume web deployment
   - ✅ Use relative markdown links: `[Section](tooling.md)`
   - ✅ Use code block language tags: `` ```bash `` or `` ```python ``
   - ❌ No `/home/user/...` or `file:///` URLs
   - ❌ No untagged code blocks (MD040 lint fails)

2. **Diagrams & Visuals** — Use mermaid for flowcharts, architecture, state diagrams

3. **Examples & Snippets** — Always tag with language and context

4. **Table of Contents** — Maintain clear structure for `mkdocs.yml`
   - Ensure all `.md` files are indexed
   - Keep nav in logical order (Architecture → Tooling → CI/CD → Advanced)

---

## 🏗️ Build & Tooling

### Just Recipes (Preferred)
- **All major tasks via `just`** — No raw cmake/make commands
- **Every recipe documented** — Docstring comments visible in `just help`

### Justfile Simplicity Rule

- Keep `justfile` recipes as orchestration glue (high-level command chaining).
- Move non-trivial shell logic to versioned scripts under `scripts/` and call them from `just` recipes.
- Exception: 1-3 straightforward shell lines are acceptable inline.

### CMake (Dependency Management)
- CMake used for: dependency discovery, build configuration, target definitions
- Do NOT use CMake for task automation → use `just` instead

### Compiler Strategy
- **Primary:** Clang (for LLVM tooling, coverage, sanitizers)
- **Optional:** GCC builds supported, not required

### Standard Tools
- **Format:** `clang-format` (C/GLSL)
- **Lint:** `clang-tidy` (C), `shellcheck` (shell), `yamllint` (YAML), `hadolint` (Docker)
- **Coverage:** `llvm-cov` (preferred), `gcovr` (alternative)
- **Test:** CTest (CMake native)
- **Text processing:** `sed`, `tr`, `column`, `awk`, `grep` (NOT custom Python scripts)

### Principle: Minimize Custom Scripts

Strongly prefer existing tools over custom scripts. Move complex logic to `scripts/` only when no standard tool exists.

---

## 🔄 Typical Feature Workflow

1. **Consult project board** — Pick an issue from the current milestone
2. **Impact assessment** — Analyze expected gains, risks, perf profile (see Golden Rule 9). Get user go/no-go
3. **Create feature branch** (`feat/my-feature`)
4. **Code implementation** — Write code + tests + docs
5. **Quality gate** — `just format && just lint && just test-all`
6. **Commit** — SoC, Conventional Commits format
7. **Local CI** — `just ci-docker-all`
8. **Open PR** — Labels + milestone + project link + `Closes #N` (only on explicit user request)
9. **Monitor CI** — Local + remote in parallel
10. **NEVER** `git push origin master`

---

## 📊 Coverage & Metrics

### Target Coverage
- **Lines:** >78% (current baseline)
- **Functions:** >85%
- **Branches:** >50%

### Measuring Coverage

```bash
just coverage-llvm  # Recommended (LLVM-Cov)
just coverage       # Alternative (GCovr)
```

### Reports Location
- LLVM-Cov: `build/coverage-llvm/coverage_report/index.html`
- GCovr: `build/coverage/reports/{coverage.txt,coverage.xml,coverage.html}`

---

## 🐛 Debugging

```bash
just ci-docker-all              # Full local CI
just build-asan && just test-asan  # Sanitizer debugging
just test-validation-layers     # Vulkan validation layers
```

---

## 🚫 Anti-Patterns

- ❌ Commit without running `just format && just lint && just test-all`
- ❌ Use raw `cmake` commands instead of `just` recipes
- ❌ Push to `origin/master` without user approval
- ❌ Add local filesystem links in docs
- ❌ Create untagged code blocks in markdown
- ❌ Make docs-only changes without updating `mkdocs.yml`
- ❌ Forget to update docs when changing behavior
- ❌ Ignore failing CI checks
- ❌ Start work without checking the project board

---

## 📞 Questions?

- Check existing commits: `git log --oneline`
- Check existing docs: `docs/` folder
- Run `just help` for available recipes
- Consult `mkdocs.yml` for doc structure
- Review project board: https://github.com/users/yoyonel/projects/3
- Label reference: [docs/github-settings.md](../../docs/github-settings.md)
