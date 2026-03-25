# Guide de staging de la documentation

Ce document explique comment prévisualiser la documentation sur une PR avant la fusion.

## Aperçu via Surge.sh

Le projet utilise [Surge.sh](https://surge.sh) pour déployer automatiquement la documentation sur chaque Pull Request. Cela permet de valider les modifications de documentation avant la fusion dans `master`.

## Configuration du token

### 1. Obtenir un token Surge

```bash
# Installer Surge
npm install -g surge

# Se connecter / créer un compte
surge login

# Obtenir le token
surge token
```

### 2. Configurer le secret GitHub

Ajoutez le token comme secret dans les paramètres du dépôt GitHub :
- `Settings` → `Secrets and variables` → `Actions`
- Créer un secret nommé `SURGE_TOKEN`

## Intégration dans le workflow

Le workflow de documentation (`.github/workflows/docs.yml`) déploie automatiquement sur Surge.sh pour chaque PR :

```yaml
- name: Deploy to Surge
  if: github.event_name == 'pull_request'
  run: |
    surge ./site ${{ github.event.pull_request.number }}-preview.surge.sh \
      --token ${{ secrets.SURGE_TOKEN }}
```

L'URL de prévisualisation est publiée en commentaire sur la PR :

```
📚 Documentation preview: https://123-preview.surge.sh
```

## Nettoyage

Les déploiements Surge sont automatiquement supprimés à la fermeture/fusion de la PR :

```yaml
on:
  pull_request:
    types: [closed]
steps:
  - run: surge teardown ${{ github.event.pull_request.number }}-preview.surge.sh
```

## Voir aussi

- [cicd_pipeline.md](./cicd_pipeline.md) — Pipeline complet CI/CD
- [documentation_system.md](./documentation_system.md) — Vue d'ensemble du système de documentation
