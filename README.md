# Suckless-OGL

[![CI/CD Pipeline](https://github.com/yoyonel/suckless-ogl/actions/workflows/main.yml/badge.svg)](https://github.com/yoyonel/suckless-ogl/actions)
[![Coverage Report](https://img.shields.io/badge/coverage-report-brightgreen)](https://yoyonel.github.io/suckless-ogl/)
[![CodeQL Status](https://github.com/yoyonel/suckless-ogl/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/yoyonel/suckless-ogl/actions/workflows/github-code-scanning/codeql)
[![Latest Release](https://img.shields.io/github/v/release/yoyonel/suckless-ogl?include_prereleases&label=release&color=blue)](https://github.com/yoyonel/suckless-ogl/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**Suckless-OGL** est un moteur de rendu 3D minimaliste écrit en C. Fidèle à la philosophie "suckless", il privilégie un code source compact, une gestion rigoureuse des ressources et une absence de dépendances superflues. Il implémente un pipeline moderne basé sur **OpenGL 4.4 Core Profile**.

## 🚀 Fonctionnalités
- **Minimalisme** : Architecture légère centrée sur la performance et la lisibilité.
- **Rendu Moderne** : Support des Skyboxes, IcoSpheres, textures et éclairage de Phong.
- **Shaders Dynamiques** : Chargement et compilation de fichiers GLSL (vertex/fragment).
- **Environnement Isolé** : Support natif de `distrobox` pour garantir un environnement de compilation reproductible.
- **Qualité & Tests** : Suite de tests unitaires, couverture de code et analyse statique via `clang-tidy`.

## 🛠️ Compilation et Utilisation

Le projet utilise un wrapper `Makefile` qui pilote `CMake` pour simplifier les interactions.

### Drapeaux de Compilation & Environnement
Le build est configuré avec les réglages suivants :
- **Optimisation** : `-Wall -Wextra -O2` pour un code propre et performant.
- **Standard POSIX** : `-D_POSIX_C_SOURCE=199309L` pour le support de `clock_gettime`.
- **Analyse Statique** : Intégration de `clang-tidy` avec des filtres d'en-têtes stricts.
- **Conteneurisation** : Utilisation par défaut de `distrobox` avec l'image `clang-dev` pour isoler les dépendances.

### Commandes principales
| Commande | Action |
| :--- | :--- |
| `make all` | Compile le projet (génère GLAD et le binaire `app`). |
| `make run` | Compile et lance l'application immédiatement. |
| `make test` | Exécute la suite de tests unitaires via `ctest`. |
| `make format` | Applique le formatage `clang-format` sur `src`, `include` et `tests`. |
| `make lint` | Lance l'analyse statique `clang-tidy` sur les fichiers sources. |
| `make coverage` | Génère un rapport HTML complet via `llvm-cov` dans `build-coverage/`. |

## 🤖 Workflow CI/CD (GitHub Actions)

Le pipeline est structuré pour optimiser le build tout en garantissant une qualité maximale :

1. **Test & Coverage** : Compilation instrumentée et exécution des tests sous **Xvfb** (serveur X virtuel). Un rapport de couverture est généré et sauvegardé en artefact.
2. **Lint & Format Check** :
   - Vérifie que le code est formaté. Si `make format` modifie un fichier, le CI échoue.
   - Lance `make lint` pour valider la conformité CERT et la sécurité.
3. **Build & Release** :
   - Se déclenche sur `master` ou sur les tags `v*`.
   - Package le binaire `app` avec les dossiers `assets/` et `shaders/`.
   - Compresse le tout dans une archive `.tar.gz` et crée une **GitHub Release** automatique.

## 📁 Structure du Projet
- `src/` & `include/` : Cœur du moteur (Log, App, Shader, Texture, Icosphere).
- `shaders/` : Sources GLSL (Phong, Background/Skybox).
- `assets/` : Ressources HDR et textures.
- `tests/` : Tests unitaires (Icosphere, Shader, Skybox, Texture, Log).
- `docs/` : Documentation technique approfondie.

## 📦 Docker / Podman
Pour tester l'application dans un conteneur avec redirection X11:
```bash
make docker-build
make docker-run
```
(Nécessite un serveur X local et les permissions xhost configurées).

📄 Licence

Ce projet est sous licence MIT. Voir le fichier LICENSE pour plus de détails.
