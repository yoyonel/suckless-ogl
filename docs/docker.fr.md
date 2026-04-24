# Support Docker

Ce projet inclut un environnement de build et d'exécution conteneurisé pour des builds cohérents et reproductibles sur différents systèmes. La configuration Docker utilise une architecture multi-étapes avec mise en cache et support du rendu headless.

## Prérequis

- **Docker** ou **Podman** installé (auto-détecté par le Justfile)
- Support BuildKit activé (par défaut dans Docker/Podman modernes)

## Démarrage rapide

### Construire l'image

```bash
just docker-build
```

Cela crée une image conteneur optimisée avec :

- Construction multi-étapes (builder + runtime minimal)
- Persistance du cache CMake
- Xvfb pour le rendu OpenGL headless

### Lancer l'application

```bash
# Rendu logiciel (Mesa llvmpipe)
just docker-run

# Rendu accéléré GPU (Intel/AMD)
just docker-run-gpu
```

Lance l'application dans un conteneur avec transfert X11 vers l'affichage hôte.

## Architecture

### Construction multi-étapes

Le [`Dockerfile`](https://github.com/yoyonel/suckless-ogl/blob/main/Dockerfile) utilise deux étapes :

#### Étape 1 : Builder (fedora:41)

- Chaîne de compilation complète (clang, cmake, ninja, git)
- **Montage cache BuildKit** sur `/src/build` pour les builds incrémentaux
- Compilation en mode Release avec build parallèle
- Copie uniquement le binaire final vers `/tmp/app`

```dockerfile
RUN --mount=type=cache,target=/src/build \
    cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTS=OFF \
    && cmake --build build --parallel \
    && cp build/app /tmp/app
```

#### Étape 2 : Runtime (fedora:41)

- Dépendances runtime minimales uniquement :
  - `glfw` — Gestion des fenêtres et entrées
  - `mesa-libGL`, `mesa-libEGL`, `mesa-dri-drivers` — Rendu OpenGL logiciel
  - `mesa-vulkan-drivers` — Support Vulkan/DRI pour le passthrough GPU
  - `xorg-x11-server-Xvfb` — Framebuffer virtuel pour le rendu headless
  - `gamemode` — Bibliothèque runtime pour le mode Performance
- Utilisateur non-root (`appuser`) pour la sécurité
- Contient uniquement : binaire, assets, shaders, script d'entrée

### Rendu headless avec Xvfb

Le script [`entrypoint.sh`](https://github.com/yoyonel/suckless-ogl/blob/main/entrypoint.sh) gère l'affichage virtuel :

```bash
#!/bin/bash
set -e
Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
sleep 2
export DISPLAY=:99
./app
```

### Optimisation du contexte de build

Le fichier `.dockerignore` exclut agressivement les fichiers non nécessaires du contexte de build.
Seuls les fichiers sources essentiels, shaders, assets et la configuration CMake sont envoyés à Docker :

```text
build/          # Artefacts de build
build-*/        # Builds coverage/debug
_deps/          # Cache FetchContent CMake (reconstruit dans le conteneur)
deps/           # Dépendances pré-compilées
.git/           # Historique Git
docs/           # Documentation
site/           # Sortie MkDocs
tests/          # Suite de tests (désactivée via -DBUILD_TESTS=OFF)
Testing/        # Artefacts CTest
*.md            # Fichiers Markdown
*.profraw       # Données de profilage LLVM
*.profdata      # Données de couverture LLVM
```

Cela réduit le transfert de contexte de **~870 Mo à ~2 Mo**, accélérant considérablement `docker build`.

!!! note "Tests désactivés dans le conteneur"
    Le build conteneur utilise `-DBUILD_TESTS=OFF` car le répertoire `tests/` est exclu
    du contexte de build. C'est intentionnel — les tests unitaires s'exécutent nativement via `just test-all`.

## Passthrough GPU

Par défaut, `just docker-run` utilise le **rendu logiciel** (Mesa llvmpipe) via Xvfb.
Pour le rendu accéléré matériellement, utilisez `just docker-run-gpu` qui passe le GPU hôte
au conteneur via le DRI (Direct Rendering Infrastructure).

### Prérequis

- Hôte Linux avec GPU Intel ou AMD
- Répertoire `/dev/dri` accessible
- Utilisateur dans le groupe `video` sur l'hôte
- Affichage X11 disponible (`$DISPLAY` défini)

### Utilisation

```bash
just docker-run-gpu
```

Cela monte le périphérique GPU de l'hôte et ajoute le groupe `video` :

```bash
docker run --rm -it \
    --device /dev/dri \
    --group-add video \
    ...
    suckless-ogl:latest /bin/bash -c "export DISPLAY=$DISPLAY && ./app"
```

### Comparaison de performance

| Mode | Renderer | Temps IBL BRDF LUT |
|------|----------|---------------------|
| Logiciel (`docker-run`) | llvmpipe (Mesa) | ~1000 ms |
| GPU (`docker-run-gpu`) | Mesa Intel Iris Xe | ~88 ms |

!!! warning "GPUs NVIDIA"
    Les GPUs NVIDIA nécessitent le [NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/overview.html)
    et `--gpus all` au lieu de `--device /dev/dri`. Cela n'est pas encore supporté par le projet.

## Cibles Justfile

### Cibles de build et d'exécution

| Cible | Description | Commande `just` |
|-------|-------------|-----------------|
| Build Image | Construction avec cache de couches | `just docker-build` |
| Build No Cache | Reconstruction complète forcée | `just docker-build-no-cache` |
| Run (Logiciel) | Exécution avec transfert X11 (Mesa llvmpipe) | `just docker-run` |
| Run (GPU) | Exécution avec passthrough GPU hôte (Intel/AMD) | `just docker-run-gpu` |

### Cibles de maintenance

| Cible | Description | Commande `just` |
|-------|-------------|-----------------|
| Clean Dangling | Suppression des images orphelines | `just docker-clean` |
| Clean All | Purge de toutes les images et du cache | `just docker-clean-all` |
| Disk Usage | Statistiques d'utilisation disque | `just docker-usage` |

## Utilisation avancée

### Moteur de conteneur personnalisé

Le Justfile auto-détecte Docker ou Podman. Pour forcer un moteur spécifique :

```bash
container_engine=podman just docker-build
```

### Builds incrémentaux

Grâce aux montages cache BuildKit, les builds suivants sont rapides :

```bash
# Premier build : ~2-3 minutes (fetch deps, compilation complète)
just docker-build

# Modifier src/shader.c
# Deuxième build : ~10-20 secondes (recompile uniquement les fichiers modifiés)
just docker-build
```

### GameMode & priorité temps réel

Pour activer le **mode Performance** (SCHED_FIFO) et GameMode dans le conteneur, des permissions spécifiques sont nécessaires :

- `--cap-add=SYS_NICE` : Permet au conteneur de définir des politiques d'ordonnancement temps réel
- `--ulimit rtprio=99` : Permet à l'utilisateur non-root de demander la priorité temps réel
- **Montage D-Bus** : Essentiel pour que `libgamemode` communique avec le démon GameMode de l'hôte

## Dépannage

### Le cache de build ne fonctionne pas

Assurez-vous que BuildKit est activé :

```bash
# Docker
export DOCKER_BUILDKIT=1

# Podman (activé par défaut)
```

### Xvfb ne démarre pas

Vérifiez que le port :99 est disponible :

```bash
docker run --rm -it suckless-ogl /bin/bash
# Dans le conteneur :
Xvfb :99 -screen 0 1920x1080x24
```

### Permission X11 refusée

Réinitialisez les permissions xhost :

```bash
xhost +local:
just docker-run
```

## CI/CD local

Pour reproduire exactement le pipeline GitHub Actions localement :

```bash
just ci-docker-all
```

## Voir aussi

- [build.md](./build.md) — Compilation sans Docker
