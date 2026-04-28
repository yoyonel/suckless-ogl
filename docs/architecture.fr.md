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

Traite les événements clavier et souris via les callbacks GLFW. Utilise `AppInputContext` (bundle de pointeurs ciblés) pour se découpler de l'objet God `App`, suivant le même pattern que `PostProcessInputContext`.

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
    └── EffectContext  (seam vers les effets individuels)
```

### Décomposition de Scene (SceneVisuals, SceneSimulation, SceneLighting)

La structure `Scene` est progressivement décomposée en sous-structs par domaine :

- **`SceneVisuals`** (`include/scene.h`) : regroupe les effets visuels — `Skybox`, `TrailRenderer`, `ShockwaveRenderer`. Accès via `scene->visuals.skybox`, etc.
- **`SceneSimulation`** (`include/scene.h`) : regroupe l'état N-body — `NBodySim`, `nbody_mode`. Accès via `scene->simulation.nbody_sim`, etc.
- **`SceneLighting`** (`include/scene.h`) : regroupe IBL, probes et matériaux — `IBLCoordinator`, `LightProbeGrid`, `MaterialLib*`. Accès via `scene->lighting.ibl_coord`, etc.

Cela réduit le nombre de champs directs de `Scene` et localise les changements par domaine.

### Décomposition de App (AppProfiling)

La structure `App` est progressivement décomposée en sous-structs par domaine :

- **`AppProfiling`** (`include/app.h`) : regroupe le profiling et les métriques — `GPUProfiler`, `GPUProfilerUI`, `FpsCounter`, `TracyManager`, `GPUUsageMonitor`, `PerfModeContext`, `perf_mode_active`, `log_gpu_metrics`. Accès via `app->profiling.gpu_profiler`, etc.

Cela réduit le nombre de champs directs de `App` (8 champs → 1 sous-struct) et localise les changements liés au monitoring de performance.

### Découplage des effets (EffectContext)

Les effets de post-traitement (bloom, DoF, auto-exposition, flou de mouvement, LUT, LUT viz) sont progressivement découplés de l'objet God `PostProcess` via un seam `EffectContext` :

- **`EffectContext`** (`include/effects/effect_context.h`) : snapshot read-only de l'état partagé du pipeline (texture source, dimensions viewport, textures profondeur/vélocité, exposition).
- Les effets reçoivent `(FX*, Params*, const EffectContext*)` au lieu de `PostProcess*`.
- Cela élimine la dépendance bidirectionnelle : `postprocess.h` → `fx_*.h` (pour l'embedding des structs) reste, mais `fx_*.c` → `postprocess.h` est supprimé.
- Actuellement migré : **bloom**. Les effets restants suivront le même pattern.

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
