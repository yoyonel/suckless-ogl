---
description: "Use when creating PRs, closing issues, updating milestones, or managing GitHub project board items. Covers label assignment, project tracking, and milestone lifecycle."
---
# GitHub Project Management

## Session Startup (MANDATORY)

At the start of **every** session, run:

```bash
gh issue list --state open
gh api repos/yoyonel/suckless-ogl/milestones?state=all --jq '.[] | "\(.number) \(.title) \(.state) \(.open_issues)/\(.open_issues + .closed_issues)"'
```

Review the project board: https://github.com/users/yoyonel/projects/3

## PR Workflow

Every PR **MUST** have:
- **Labels**: at least one scope label (`scope:*`, `subsystem:*`) + one type label (`bug`, `enhancement`, `refactor`, `quality`, `security`, `documentation`)
- **Milestone**: linked to the relevant phase
- **Project**: added to "suckless-ogl Roadmap" (project #3)
- **Linked issues**: use `Closes #N` in PR body for auto-closing

Label reference: see [docs/github-settings.md](../../docs/github-settings.md).

## Issue Lifecycle

1. When starting work on an issue: move to **In Progress** on the project board
2. Create a feature branch: `feat/<description>`, `fix/<description>`, etc.
3. Implement in small, focused commits (SoC)
4. Open PR with labels + milestone + project link + `Closes #N`
5. CI must be green before merge (local `just ci-docker-all` + remote GitHub Actions)
6. After merge: verify the linked issue auto-closed; if not, close manually
7. When all issues in a milestone are closed: close the milestone

## Branch Naming

| Prefix | Usage |
|--------|-------|
| `feat/<description>` | New features (shader, effect, control, UI) |
| `fix/<description>` | Bug fixes |
| `refactor/<description>` | Code restructuring |
| `perf/<description>` | Performance optimization |
| `docs/<description>` | Documentation-only changes |
| `ci/<description>` | CI/CD pipeline changes |
| `test/<description>` | Test additions/modifications |

## Milestone Lifecycle

- Milestones represent **themed phases** of the project roadmap
- Each milestone has a description stating its scope and acceptance criteria
- A milestone is closed when **all its issues are resolved** and CI is green
- Do not reopen closed milestones — create a new one if rework is needed

## Project Board Columns

| Column | Description |
|--------|-------------|
| **Backlog** | Issues identified but not yet prioritized |
| **Todo** | Prioritized for the current or next milestone |
| **In Progress** | Actively being worked on (max 2–3 items) |
| **In Review** | PR opened, awaiting CI validation |
| **Done** | Merged and verified |
