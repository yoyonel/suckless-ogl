# Configuration Docker

Ce document décrit la configuration Docker du projet pour le développement et la CI/CD.

## Architecture multi-étapes

Le `Dockerfile` utilise une construction multi-étapes pour optimiser la taille de l'image finale et séparer les dépendances de build des dépendances de runtime.

### Étape 1 : Builder

```dockerfile
FROM debian:13-slim AS builder
RUN apt-get update && apt-get install -y \
    cmake clang make \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

Cette étape contient tous les outils de compilation et télécharge les dépendances via FetchContent.

### Étape 2 : Runtime

```dockerfile
FROM debian:13-slim AS runtime
# Copier uniquement le binaire et les shaders
COPY --from=builder /build/app /app/app
COPY --from=builder /build/shaders /app/shaders
COPY --from=builder /build/assets /app/assets
```

L'image finale ne contient que le binaire compilé, les shaders et les assets.

## Rendu headless avec Xvfb

Étant donné que les conteneurs Docker n'ont pas d'affichage, nous utilisons **Xvfb** (X Virtual Framebuffer) pour simuler un affichage :

```bash
# Démarrer Xvfb
Xvfb :99 -screen 0 1920x1080x24 &
export DISPLAY=:99

# Lancer l'application
./app
```

Le script `entrypoint.sh` gère automatiquement le démarrage de Xvfb.

## Cibles Makefile / Just

| Commande | Description |
|---------|-------------|
| `just docker-build` | Construction de l'image (multi-étapes) |
| `just docker-run` | Exécution avec transfert X11 |
| `just docker-test` | Tests dans le conteneur |
| `just docker-clean-all` | Nettoyage complet (images, conteneurs, cache) |
| `just ci-docker-all` | Pipeline CI complet (lint + build + test) |

## Cache BuildKit

Pour accélérer les rebuilds, le projet utilise les montages de cache BuildKit :

```dockerfile
RUN --mount=type=cache,target=/var/cache/apt \
    apt-get update && apt-get install -y cmake ...
```

Cela évite de re-télécharger les paquets apt à chaque build.

## Permissions GameMode

Si GameMode est disponible sur l'hôte, le conteneur peut l'utiliser pour améliorer les performances :

```bash
# Ajouter l'utilisateur au groupe gamemode
sudo usermod -aG gamemode $USER

# Exécuter avec les permissions étendues
docker run --device /dev/dri --group-add gamemode app
```

## CI/CD local

Pour reproduire exactement le pipeline GitHub Actions localement :

```bash
# Exécuter tous les jobs CI dans Docker
just ci-docker-all
```

Cette commande enchaîne :
1. `just ci-docker lint` — Analyse statique
2. `just ci-docker Release` — Build + tests Release
3. `just ci-docker Debug` — Build + tests Debug avec sanitizers

## Voir aussi

- [cicd_pipeline.md](./cicd_pipeline.md) — Pipeline GitHub Actions
- [build.md](./build.md) — Compilation sans Docker
