---
description: "Use when making commits, creating branches, or planning work iterations. Covers SoC commit discipline, Conventional Commits format, pre-commit workflow, and branching strategy."
applyTo: ["src/**/*.c", "include/**/*.h", "shaders/**", "tests/**/*.c", "Justfile", "CMakeLists.txt"]
---
# Commit & Workflow Discipline

## Separation of Concerns (SoC) — MANDATORY

Every commit must have a **single concern**. Never mix:
- Application code (`feat:`, `fix:`, `refactor:`) with CI (`ci:`) or docs (`docs:`)
- Test changes (`test:`) with the code they test (separate commits when possible)
- Script/tooling changes (`chore:`) with application logic

## Conventional Commits Format

Follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) strictly:

```text
<type>(<scope>): <subject>

<body>

<footer>
```

**Types** (based on project specifics):
- `feat`: New feature (keyboard control, shader, logging, etc.)
- `fix`: Bug fix
- `refactor`: Code restructuring without feature change
- `test`: Test suite additions/modifications
- `docs`: Documentation only
- `build`: Build system, CMake, dependencies
- `ci`: GitHub Actions, Docker, CI pipeline
- `chore`: Dependencies, config, tooling, git config
- `perf`: Performance optimization

**Scope** (optional but recommended):
- `logging`, `controls`, `shader`, `memory`, `ci`, `docs`, `coverage`, `rendering`, `postprocess`, `ibl`, `profiling`, etc.

**Examples:**

```text
feat(controls): Add F11 fullscreen toggle with state restoration

fix(logging): Correct thread ID formatting in log messages

docs(coverage): Add LLVM-Cov section with KPI metrics

ci(github-actions): Add coverage-report-llvm job with artifact uploads

refactor(engine): Extract shader compilation logic to separate function
```

## Pre-Commit Workflow

1. Run `just pre-commit-install` (one-time setup)
2. Make code changes
3. Run `just format` → fixes formatting automatically
4. Run `just lint` → must pass with no errors and no warnings
5. Run `just test-all` → all tests must pass
6. Run `just pre-push-run` → final check before commit
7. Run `git status --short --branch` and review all local changes
8. Ensure all remaining files are either:
   - intentionally staged for the current commit, or
   - explicitly deferred and communicated as next step
9. Use `git add` + `git commit` (hooks will verify format/lint/tests)
10. **NEVER** `git push` — wait for user approval

If any hook fails:
- Read the error message
- Fix the issue
- Retry the commit

### Warning Handling Policy

- A lint command with `exit code 0` is NOT considered successful if warnings are present.
- Warnings must be treated and fixed before moving to the next step or committing.
- Use output checks when needed: `just lint 2>&1 | grep -i warning`
- `clang-tidy` warnings (including `performance-*`) are blocking and must be fixed, not deferred.
- During GitHub Actions monitoring, always run `just ci-docker-all` locally in parallel and fix local warnings/errors immediately to preempt remote failures.

## Git Safety Rules

- **Never `git push` without explicit user authorization**
- **Never `git push --force`** without explicit user authorization
- **Never `git push origin master`** — all changes go through feature branches + PRs
- **Never delete untracked files** without asking
- Prefer `git stash` over discarding changes

## Commit Checklist

Before running `git commit`:

- [ ] Code formatted: `just format` ✅
- [ ] No lint errors or warnings: `just lint` ✅
- [ ] All tests pass: `just test-all` ✅
- [ ] Git working tree reviewed: `git status --short --branch` ✅
- [ ] No unexpected leftovers (unstaged/untracked) without explicit handling plan
- [ ] Docs created/updated (if feature/design change)
- [ ] `mkdocs.yml` updated (if new doc added)
- [ ] No local filesystem links in docs
- [ ] Commit message follows Conventional Commits format
- [ ] Pre-commit hooks pass: `just pre-commit-run` ✅

Before handing back control in chat/prompt:

- [ ] Re-check repo state with `git status --short --branch`
- [ ] Clearly report what is done, what remains modified, and what is deferred
