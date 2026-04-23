# Pipeline CI/CD

Ce document décrit le pipeline d'intégration et de déploiement continus basé sur GitHub Actions.

## Vue d'ensemble

Le pipeline est défini dans `.github/workflows/` et s'exécute automatiquement sur chaque push et pull request vers la branche `master`.

## Jobs du workflow

### 1. Lint

Analyse statique du code source :

- **clang-tidy** : Analyse C/C++
- **check_nolint.sh** : Garde anti-suppression NOLINT (voir [linting_strategy.fr.md](linting_strategy.fr.md#politique-anti-suppression))
- **shellcheck** : Vérification des scripts shell
- **yamllint** : Validation des fichiers YAML
- **hadolint** : Lint du Dockerfile

```yaml
lint:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - run: just lint
```

### 2. Test

Exécution de la suite de tests avec Xvfb (rendu headless) :

```yaml
test:
  runs-on: ubuntu-latest
  steps:
    - run: just test-all
```

### 3. Docs

Construction et validation de la documentation :

```yaml
docs:
  runs-on: ubuntu-latest
  steps:
    - run: just docs
```

### 4. Build (matrice)

Construction en mode Release et Debug, avec et sans sanitizers :

| Configuration | Compilateur | Flags |
|--------------|------------|-------|
| Release/Clang | Clang 18 | `-O3` |
| Debug/Clang | Clang 18 | `-g -fsanitize=address,undefined` |
| Release/GCC | GCC 13 | `-O3` |

### 5. Deploy

Déploiement de la documentation sur GitHub Pages (branche `master` uniquement) :

```yaml
deploy:
  needs: [lint, test, docs]
  if: github.ref == 'refs/heads/master'
  steps:
    - uses: peaceiris/actions-gh-pages@v4
```

### 6. Release

Création automatique des artefacts de release pour les tags `v*.*.*` :

- Binaire Linux compilé statiquement
- Archive des shaders et assets
- Changelog extrait depuis `CHANGELOG.md`

## Releases nocturnes et stables

| Type | Déclencheur | Étiquette |
|------|------------|---------|
| **Stable** | Tag `v*.*.*` | `latest` |
| **Nightly** | Cron 02h00 UTC | `nightly` |

## Validation locale

Avant de pousser, répliquez le pipeline CI localement :

```bash
# Équivalent du CI complet via Docker
just ci-docker-all
```

## Voir aussi

- [docker.md](./docker.md) — Environnement Docker utilisé par le CI
- [linting_strategy.md](./linting_strategy.md) — Détails de la stratégie de lint
- [justfile.md](./justfile.md) — Recettes d'automatisation
