# GitHub Settings — suckless-ogl

Configuration de référence du repo [yoyonel/suckless-ogl](https://github.com/yoyonel/suckless-ogl).

---

## Labels

### Labels génériques (conservés)

| Label | Couleur | Description |
|-------|---------|-------------|
| `bug` | `#d73a4a` | Something isn't working |
| `documentation` | `#0075ca` | Improvements or additions to documentation |
| `enhancement` | `#a2eeef` | New feature or request |

### Labels `subsystem:*`

| Label | Couleur | Description |
|-------|---------|-------------|
| `subsystem:rendering` | `#1d76db` | Rendering pipeline, draw calls, instancing, SSBO |
| `subsystem:postprocess` | `#5319e7` | Post-processing effects (bloom, DoF, exposure, FXAA, etc.) |
| `subsystem:shader` | `#006b75` | GLSL shaders, compute shaders, shader compilation |
| `subsystem:ibl` | `#0e8a16` | Image-Based Lighting, PBR, irradiance, specular maps |
| `subsystem:profiling` | `#e36209` | GPU profiler, Tracy, perf timers, benchmarks |
| `subsystem:ui` | `#795548` | UI overlay, debug visualization, help screen |
| `subsystem:input` | `#c2e0c6` | Keyboard/mouse input, camera controls, key bindings |
| `subsystem:async` | `#bfd4f2` | Async loading, environment manager, IO |

### Labels `infra:*`

| Label | Couleur | Description |
|-------|---------|-------------|
| `infra:ci` | `#fbca04` | GitHub Actions, Docker CI, workflows |
| `infra:build` | `#333333` | CMake, Justfile, compilation toolchain |
| `infra:docker` | `#2b67c6` | Docker images, CI containers |

### Labels `scope:*`

| Label | Couleur | Description |
|-------|---------|-------------|
| `scope:performance` | `#e36209` | Performance optimization, GPU utilization |
| `scope:visual-regression` | `#d4c5f9` | Visual regression testing, reference images |
| `scope:cross-platform` | `#c5def5` | Windows/Linux portability |

### Labels transversaux

| Label | Couleur | Description |
|-------|---------|-------------|
| `quality` | `#bfdadc` | Lint, format, clang-tidy, tests, hooks |
| `refactor` | `#c5def5` | Restructuration, architecture |
| `security` | `#d93f0b` | Security hardening, input validation, path traversal |

### Labels supprimés (defaults GitHub)

Les labels par défaut suivants doivent être supprimés :
`duplicate`, `good first issue`, `help wanted`, `invalid`, `question`, `wontfix`.

---

## Milestones

Milestones correspondant aux phases du roadmap projet.

| # | Milestone | État | Description |
|---|-----------|------|-------------|
| 1 | **Phase 1 — Compute Pipeline Migration** | 🔵 Open | Migrate remaining raster-only post-process effects to compute shaders (auto-exposure dual-path ✅, bloom, DoF). Validate perf parity on iGPU + dGPU. |
| 2 | **Phase 2 — Test & Quality Hardening** | 🔵 Open | Fix build blockers (missing test files), eliminate NOLINTNEXTLINE suppressions, unify integration test pipeline (PBO everywhere), achieve >85% coverage. |
| 3 | **Phase 3 — CI Industrialization** | 🔵 Open | Branch protection rules, required status checks, PR template enforcement, Windows CI parity, coverage thresholds in CI. |
| 4 | **Phase 4 — Advanced Rendering** | 🔵 Open | Advanced bokeh DoF, cinematic LUT pipeline, dynamic IBL probes, screen-space reflections. As documented in `docs/advanced_bokeh_effects.md`, `docs/ibl_architecture_ideas.md`. |
| 5 | **Phase 5 — Documentation & DX** | 🔵 Open | Multilingual doc sync (all FR translations), mkdocs full coverage, API doc generation (Doxygen), developer onboarding guide. |

---

## Project Board

| Champ | Valeur |
|-------|--------|
| **Nom** | suckless-ogl Roadmap |
| **Numéro** | #3 |
| **ID** | `PVT_kwHOAGrT4s4BUfHP` |
| **Type** | GitHub Projects v2 (user-scoped, linked au repo) |
| **URL** | https://github.com/users/yoyonel/projects/3 |

> **Note** : Les GitHub Projects v2 sont créés au niveau utilisateur/organisation,
> pas au niveau repo. Pour qu'un projet apparaisse dans l'onglet **Projects** d'un repo,
> il doit être explicitement lié via l'API GraphQL `linkProjectV2ToRepository`.

---

## Reproduction

Commandes `gh` pour recréer cette configuration :

```bash
# --- Labels ---
# Supprimer les defaults inutiles
for label in "duplicate" "good first issue" "help wanted" "invalid" "question" "wontfix"; do
  gh label delete "$label" --yes 2>/dev/null
done

# Créer les labels subsystem
gh label create "subsystem:rendering"   --color "1d76db" --description "Rendering pipeline, draw calls, instancing, SSBO"
gh label create "subsystem:postprocess" --color "5319e7" --description "Post-processing effects (bloom, DoF, exposure, FXAA, etc.)"
gh label create "subsystem:shader"      --color "006b75" --description "GLSL shaders, compute shaders, shader compilation"
gh label create "subsystem:ibl"         --color "0e8a16" --description "Image-Based Lighting, PBR, irradiance, specular maps"
gh label create "subsystem:profiling"   --color "e36209" --description "GPU profiler, Tracy, perf timers, benchmarks"
gh label create "subsystem:ui"          --color "795548" --description "UI overlay, debug visualization, help screen"
gh label create "subsystem:input"       --color "c2e0c6" --description "Keyboard/mouse input, camera controls, key bindings"
gh label create "subsystem:async"       --color "bfd4f2" --description "Async loading, environment manager, IO"

# Créer les labels infra
gh label create "infra:ci"              --color "fbca04" --description "GitHub Actions, Docker CI, workflows"
gh label create "infra:build"           --color "333333" --description "CMake, Justfile, compilation toolchain"
gh label create "infra:docker"          --color "2b67c6" --description "Docker images, CI containers"

# Créer les labels scope
gh label create "scope:performance"         --color "e36209" --description "Performance optimization, GPU utilization"
gh label create "scope:visual-regression"   --color "d4c5f9" --description "Visual regression testing, reference images"
gh label create "scope:cross-platform"      --color "c5def5" --description "Windows/Linux portability"

# Créer les labels transversaux
gh label create "quality"               --color "bfdadc" --description "Lint, format, clang-tidy, tests, hooks"
gh label create "refactor"              --color "c5def5" --description "Restructuration, architecture"
gh label create "security"              --color "d93f0b" --description "Security hardening, input validation, path traversal"

# --- Milestones ---
gh api repos/yoyonel/suckless-ogl/milestones -f title="Phase 1 — Compute Pipeline Migration" \
  -f description="Migrate remaining raster-only post-process effects to compute shaders. Validate perf parity on iGPU + dGPU."
gh api repos/yoyonel/suckless-ogl/milestones -f title="Phase 2 — Test & Quality Hardening" \
  -f description="Fix build blockers, eliminate NOLINTNEXTLINE, unify integration test pipeline, achieve >85% coverage."
gh api repos/yoyonel/suckless-ogl/milestones -f title="Phase 3 — CI Industrialization" \
  -f description="Branch protection, required status checks, PR template enforcement, coverage thresholds."
gh api repos/yoyonel/suckless-ogl/milestones -f title="Phase 4 — Advanced Rendering" \
  -f description="Advanced bokeh DoF, cinematic LUT pipeline, dynamic IBL probes, screen-space reflections."
gh api repos/yoyonel/suckless-ogl/milestones -f title="Phase 5 — Documentation & DX" \
  -f description="Multilingual doc sync, mkdocs full coverage, Doxygen generation, developer onboarding guide."

# --- Project ---
gh project create --owner @me --title "suckless-ogl Roadmap"
# Puis lier au repo via GraphQL :
# gh api graphql -f query='mutation { linkProjectV2ToRepository(input: {projectId: "<PROJECT_ID>", repositoryId: "<REPO_ID>"}) { repository { id } } }'
```

---

## Branch Protection (à configurer)

| Règle | Valeur |
|-------|--------|
| **Branch** | `master` |
| **Required status checks** | `lint-and-format`, `test-and-coverage`, `build-production` |
| **Strict** | Oui (branch must be up-to-date before merge) |
| **Enforce admins** | Non |
| **Required reviews** | Non (solo project) |
| **Force push** | Interdit |
| **Delete branch** | Interdit |

```bash
gh api repos/yoyonel/suckless-ogl/branches/master/protection -X PUT \
  -H "Accept: application/vnd.github+json" \
  --input - <<'EOF'
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "lint-and-format",
      "test-and-coverage",
      "build-production"
    ]
  },
  "enforce_admins": false,
  "required_pull_request_reviews": null,
  "restrictions": null
}
EOF
```
