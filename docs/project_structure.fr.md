# Structure du projet

Ce document décrit l'organisation des fichiers et répertoires du projet.

## Arborescence

```
suckless-ogl/
├── src/                  # Code source C
│   ├── main.c            # Point d'entrée
│   ├── app.c             # Orchestration principale
│   ├── app_ui.c          # Rendu de l'interface
│   ├── app_input.c       # Gestion des entrées
│   ├── app_env.c         # Environnement HDR
│   ├── app_scene.c       # Scène 3D
│   ├── app_binding.c     # Registre des raccourcis
│   └── platform/         # Couche de portabilité
├── include/              # En-têtes C
│   ├── app.h             # Structure App principale
│   ├── app_input.h       # Seam AppInputContext
│   ├── app_input_state.h # Sous-struct AppInput
│   ├── app_profiling.h   # Sous-struct AppProfiling
│   ├── app_window.h      # Sous-struct AppWindow
│   ├── gamepad_context.h # Seam GamepadContext
│   ├── scene.h           # Agrégat Scene (inclut les sous-structs)
│   ├── scene_config.h    # SceneConfig : flags runtime et enums
│   ├── scene_gpu_resources.h # SceneGPUResources : handles GLuint
│   ├── scene_lighting.h  # SceneLighting : IBL, probes, matériaux
│   ├── scene_shaders.h   # SceneShaders : pointeurs Shader
│   ├── scene_simulation.h # SceneSimulation : état N-body
│   ├── scene_visuals.h   # SceneVisuals : skybox, trails, shockwave
│   └── ...
├── shaders/              # Shaders GLSL
│   ├── pbr.vert/.frag    # Shader PBR principal
│   ├── skybox.vert/.frag # Rendu de la skybox
│   ├── postprocess/      # Effets de post-traitement
│   └── compute/          # Shaders compute (IBL, exposition)
├── assets/               # Ressources statiques
│   ├── textures/         # Textures PNG/HDR
│   ├── materials/        # Descriptions de matériaux
│   └── fonts/            # Fontes FreeType
├── tests/                # Tests unitaires et d'intégration
├── docs/                 # Documentation MkDocs
├── scripts/              # Scripts d'automatisation
├── deps/                 # Dépendances locales (non gérées par FetchContent)
│   ├── stb/              # stb_image, stb_truetype
│   ├── cglm/             # Mathématiques vectorielles
│   └── glad/             # Chargeur OpenGL
├── CMakeLists.txt        # Configuration CMake
├── Justfile              # Recettes Just
└── Makefile              # Compatibilité make
```

## Modules et responsabilités

### `src/app.c` — Orchestration

Point central de l'application. Initialise tous les sous-systèmes, gère la boucle principale, coordonne les transitions.

### `src/app_ui.c` — Interface utilisateur

Rendu de la superposition d'informations (F1), timeline GPU (F3), aide clavier (H), notifications d'actions.

### `src/app_input.c` — Entrées

Callbacks GLFW pour clavier et souris. Dispatch des commandes via le registre de raccourcis.

### `src/app_env.c` — Environnement

Chargement asynchrone des HDR, génération IBL, transitions entre environnements.

### `src/app_scene.c` — Scène

Gestion des sphères (instanciation, tri Back-to-Front, rendu instancié).

## Référence des contrôles

| Touche | Action |
|--------|--------|
| `F1` | Basculer la superposition d'informations |
| `F3` | Basculer la timeline GPU |
| `SHIFT+F3` | Position timeline (haut/bas) |
| `F4` | Journalisation des métriques dans la console |
| `F` | Basculer plein écran |
| `H` | Aide clavier interactive |
| `M` | Basculer le mode capture souris |
| `E` | Environnement suivant |
| `SHIFT+E` | Environnement précédent |
| `T` | Basculer le mode de transition |
| `B` | Bloom on/off |
| `SHIFT+B` | Mode débogage Bloom (étapes) |
| `ALT+B` | Mode débogage Bloom (mip levels) |
| `+` / `-` | Ajuster l'exposition manuelle |
| `A` | Basculer l'exposition automatique |
| `R` | Réinitialiser l'exposition |
| `Esc` / `Q` | Quitter |

## Voir aussi

- [architecture.md](./architecture.md) — Architecture des modules
- [keyboard_system.md](./keyboard_system.md) — Système de raccourcis détaillé
- [build.md](./build.md) — Compilation
