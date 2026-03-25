# Suckless-OGL

<div align="center">
  <p>
    <a href="https://github.com/yoyonel/suckless-ogl/actions"><img src="https://github.com/yoyonel/suckless-ogl/actions/workflows/main.yml/badge.svg" alt="Pipeline CI/CD"></a>
    <a href="https://yoyonel.github.io/suckless-ogl/"><img src="https://img.shields.io/badge/coverage-report-brightgreen" alt="Rapport de couverture"></a>
    <a href="https://github.com/yoyonel/suckless-ogl/releases"><img src="https://img.shields.io/github/v/release/yoyonel/suckless-ogl?include_prereleases&label=release&color=blue" alt="Dernière version"></a>
    <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="Licence : MIT"></a>
  </p>
  <h3>Moteur de rendu PBR C11 haute performance</h3>
</div>

![Image de référence Suckless-OGL](reference_image.png)

**Suckless-OGL** est un moteur de rendu 3D minimaliste écrit en C. Fidèle à la philosophie "suckless", il privilégie une base de code compacte, une gestion rigoureuse des ressources et l'absence de dépendances inutiles. Il implémente un pipeline moderne basé sur le **profil core OpenGL 4.4**.

## 🚀 Fonctionnalités

- **Minimalisme** : Architecture légère axée sur la performance et la lisibilité.
- **Rendu moderne** : Support des Skyboxes, IcoSphères, textures et éclairage Phong.
- **Shaders dynamiques** : Chargement et compilation des fichiers GLSL (vertex/fragment).
- **Optimisation des shaders** : Compilation statique (Release) ou dynamique (Debug) pour équilibrer flexibilité et performance. [Voir la documentation](shader_optimization.md).
- **Masquage par le stencil buffer** : Optimisation du post-traitement pour différencier le skybox des objets. [Voir la documentation](stencil_masking.md).
- **Mode performance** : Optimisation adaptative (GameMode/Natif) pour une stabilité maximale du framerate. [Voir la documentation](perf_and_notifications.md).
- **Environnement isolé** : Support natif de `distrobox` pour garantir un environnement de build reproductible.
- **Qualité et tests** : Suite de tests unitaires, couverture de code, analyse statique et [mocking autonome](standalone_testing_mocking.md).
- **Overlay clavier interactif** : Interface Cyberpunk moderne avec [mise à l'échelle adaptative](keyboard_system.md) et [référence des paramètres visuels](ui_visual_parameters.md).
- **Pipeline de post-traitement** : Stack avancée incluant [Auto-Exposition](exposure_analysis.md), [Bloom](postprocess_optimizations_2026-02.md), [Debug Bloom](bloom_debug.md) et [histogrammes de debug](auto_exposure_debug_histogram.md).

## 🛠️ Compilation et utilisation

Le projet utilise un wrapper `Makefile` qui pilote `CMake` pour simplifier les interactions.
Pour une alternative moderne, voir la [documentation Justfile](justfile.md).

### Flags de compilation et environnement

Le build est configuré avec les paramètres suivants :

- **Optimisation** : `-Wall -Wextra -O2` pour un code propre et performant.
- **Standard POSIX** : `-D_POSIX_C_SOURCE=199309L` pour le support de `clock_gettime`.
- **Analyse statique** : Intégration de `clang-tidy` avec filtres stricts sur les en-têtes.
- **Conteneurisation** : Utilisation par défaut de `distrobox` avec l'image `clang-dev` pour isoler les dépendances.

### Commandes principales

| Commande | Action |
| :--- | :--- |
| `make all` | Compile le projet (génère GLAD et le binaire `app`). |
| `make debug` | Compile en mode DEBUG (shaders dynamiques, idéal pour le développement). |
| `make release` | Compile en mode RELEASE (shaders optimisés statiquement). |
| `make run` | Lance la version DEBUG. |
| `make run-release` | Lance la version RELEASE. |
| `make test` | Exécute la suite de tests unitaires via `ctest`. |
| `make test-apitrace` | Vérification légère de performance automatisée. |
| `make test-integration-apitrace` | Scénario d'intégration complet de l'application automatisé. |
| `make format` | Applique le formatage `clang-format` sur `src`, `include` et `tests`. |
| `make lint` | Lance l'analyse statique `clang-tidy` sur les fichiers source. |
| `make coverage` | Génère un rapport HTML complet via `llvm-cov` dans `build-coverage/`. |

## 🤖 Workflow CI/CD (GitHub Actions)

Le pipeline est structuré pour optimiser le build tout en garantissant une qualité maximale. Il gère les tests, l'assurance qualité, la documentation et les releases automatisées.

**[> Lire la documentation complète du pipeline CI/CD](cicd_pipeline.md)**

### Résumé rapide

1. **Tests et couverture** : Compilation instrumentée et exécution des tests sous **Xvfb**.
2. **Lint et formatage** : Application du style (`clang-format`) et analyse statique (`clang-tidy`).
3. **Releases automatisées** :
   - **Nightly** : Construite chaque nuit à 01:00 UTC et à chaque push sur master.
   - **Stable** : Déclenchée par les tags de version (`v*`).

## 📁 Structure du projet

- `src/` et `include/` : Cœur du moteur (Log, App, Shader, Texture, Icosphere).
- `shaders/` : Sources GLSL (Phong, Background/Skybox).
- `assets/` : Ressources HDR et textures.
- `tests/` : Tests unitaires (Icosphere, Shader, Skybox, Texture, Log).
- `docs/` : Documentation technique approfondie.
