# Architecture de l'application

Ce document décrit l'architecture modulaire de l'application après le refactoring de `app.c`.

## Vue d'ensemble du refactoring

Le fichier monolithique `app.c` a été décomposé en modules cohérents. Chaque module a une responsabilité claire et des interfaces bien définies.

## Modules

### `app_ui` — Interface utilisateur

Gère l'affichage de la superposition d'informations, du clavier interactif et des métriques de performance.

**Responsabilités :**
- Rendu des textes et des indicateurs visuels
- Gestion de la disposition du clavier
- Affichage de la Timeline GPU

### `app_input` — Gestion des entrées

Traite les événements clavier et souris via les callbacks GLFW.

**Responsabilités :**
- Dispatch des commandes depuis les frappes clavier
- Gestion des modes modificateurs (Shift, Alt)
- Mode Dry-Run pour l'exploration des raccourcis

### `app_env` — Environnement HDR

Coordonne le chargement des cartes d'environnement et la transition entre elles.

**Responsabilités :**
- Chargement asynchrone des HDR
- Génération des cartes IBL (irradiance, spéculaire préfiltrée)
- Transitions (fondu enchaîné, écran noir)

### `app_scene` — Scène et primitives

Gère les objets de la scène, leur tri et leur rendu.

**Responsabilités :**
- Gestion des sphères (création, mise à jour, tri)
- Appel des passes de rendu (géométrie, skybox, post-traitement)
- Coordination du profiler GPU

## Propriété des données

La structure `App` reste le point d'entrée central. Elle possède tous les sous-systèmes et leur passe des pointeurs selon le besoin.

```
App
├── AppUI          (rendu de l'interface)
├── AppInput       (événements et raccourcis)
├── AppEnv         (environnement HDR)
├── AppScene       (scène 3D)
├── GPUProfiler    (métriques temporelles GPU)
├── AsyncLoader    (chargement asynchrone)
└── PostProcess    (effets de post-traitement)
```

## Configuration CMake

Les modules sont compilés séparément et liés à l'exécutable principal :

```cmake
target_sources(app PRIVATE
    src/app.c
    src/app_ui.c
    src/app_input.c
    src/app_env.c
    src/app_scene.c
)
```

Chaque module expose son interface via un en-tête dans `include/` avec le préfixe correspondant (`app_ui.h`, `app_input.h`, etc.).
