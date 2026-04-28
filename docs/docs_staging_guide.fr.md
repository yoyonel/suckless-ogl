# Guide de staging de la documentation

Ce document explique comment prévisualiser la documentation sur une PR avant la fusion.

## Aperçu via GitHub Pages

Le projet utilise [GitHub Pages](https://pages.github.com/) pour déployer automatiquement la documentation sur chaque Pull Request. Cela permet de valider les modifications de documentation avant la fusion dans `master`.

L'action [`rossjrw/pr-preview-action`](https://github.com/rossjrw/pr-preview-action) déploie chaque preview dans un sous-dossier `pr-preview/pr-<N>/` sur la branche `gh-pages`.

## Format des URLs de preview

```text
https://yoyonel.github.io/suckless-ogl/pr-preview/pr-<NUMERO>/
```

Par exemple, la PR #42 serait accessible à :
`https://yoyonel.github.io/suckless-ogl/pr-preview/pr-42/`

## Configuration requise

Aucun token ou service externe n'est nécessaire. Le déploiement utilise le `GITHUB_TOKEN` natif fourni par GitHub Actions.

### Configuration du dépôt

1. Allez dans les **Settings** du dépôt > **Pages**.
2. Vérifiez que la source est **Deploy from a branch** (`gh-pages`).
3. Dans **Settings** > **Actions** > **General** > **Workflow permissions**, sélectionnez **Read and write permissions**.

## Intégration dans le workflow

Le workflow CI (`.github/workflows/main.yml`) contient deux jobs de déploiement :

### Preview PR (job deploy-preview)

```yaml
deploy-preview:
  needs: [documentation]
  if: github.event_name == 'pull_request' && always()
  runs-on: ubuntu-latest
  concurrency: preview-${{ github.ref }}
  steps:
    - uses: actions/checkout@v6
    - uses: actions/download-artifact@v8
      if: github.event.action != 'closed'
      with:
        name: doxygen-docs
        path: preview-site
    - uses: rossjrw/pr-preview-action@v1
      with:
        source-dir: ./preview-site/
        preview-branch: gh-pages
        umbrella-dir: pr-preview
        action: auto
        comment: true
```

### Déploiement production (job deploy-pages)

Le déploiement de production sur `master` utilise `JamesIves/github-pages-deploy-action` avec `clean-exclude: pr-preview` pour préserver les previews actives :

```yaml
- uses: JamesIves/github-pages-deploy-action@v4
  with:
    branch: gh-pages
    folder: ./public-site
    clean-exclude: pr-preview
    force: false
```

## Contenu du preview

Le preview inclut le **site complet de documentation** :

- Documentation MkDocs (architecture, guides, API)
- Référence API Doxygen
- Rapports de couverture

## Nettoyage

### Nettoyage automatique

Les previews sont automatiquement supprimées à la fermeture ou fusion d'une PR. L'événement `pull_request: closed` déclenche le job `deploy-preview` avec `action: auto`, qui détecte la fermeture et supprime le dossier `pr-preview/pr-<N>/` de `gh-pages`.

### Nettoyage manuel

Si un preview n'a pas été correctement nettoyé :

```bash
git checkout gh-pages
rm -rf pr-preview/pr-<NUMERO>
git add -A && git commit -m "chore: remove stale PR preview"
git push origin gh-pages
```

## Voir aussi

- [cicd_pipeline.md](./cicd_pipeline.md) — Pipeline complet CI/CD
- [documentation_system.md](./documentation_system.md) — Vue d'ensemble du système de documentation
