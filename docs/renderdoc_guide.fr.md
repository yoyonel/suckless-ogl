# Guide RenderDoc

Ce document décrit l'utilisation de RenderDoc pour la capture et l'analyse des frames GPU.

## Installation

### Debian 13

```bash
# RenderDoc est disponible dans les dépôts Debian 13
sudo apt install renderdoc

# Ou depuis le site officiel pour la dernière version
wget https://renderdoc.org/stable/latest/renderdoc_*.deb
sudo dpkg -i renderdoc_*.deb
```

## Configuration du profilage

### Paramètres recommandés

Lors du lancement via RenderDoc :

```
Executable:    /path/to/build/app
Working dir:   /path/to/project
Environment:   DISPLAY=:0
               MESA_GL_VERSION_OVERRIDE=4.6
               MESA_GLSL_VERSION_OVERRIDE=460
```

Options :
- ✅ **Capture child processes** : Pour capturer les subprocesses éventuels
- ✅ **Allow fullscreen** : Activer le rendu plein écran dans la capture

## Capture des frames IBL

Pour capturer une séquence de génération IBL complète :

1. Lancer l'application via RenderDoc
2. Appuyer sur `E` pour déclencher un changement d'environnement
3. Appuyer sur `F12` pour capturer les frames immédiatement après (génération IBL progressive)
4. Analyser les passes compute dans l'Event Browser

## Conseils Intel/Mesa

Sur Intel avec le driver Mesa, RenderDoc peut nécessiter des variables d'environnement supplémentaires :

```bash
# Activer le support RenderDoc dans Mesa
MESA_GLSL_CACHE_DISABLE=true \
LIBGL_DEBUG=verbose \
renderdoc ./build/app
```

Si l'application ne se lance pas, vérifier la compatibilité de la version Mesa :

```bash
glxinfo | grep "OpenGL version"
# Doit être >= 4.3 pour les shaders compute
```

## Navigation dans l'Event Browser

L'Event Browser de RenderDoc affiche la hiérarchie des appels GL :

```
Frame 1
├─ [10] glPushDebugGroup "Frame Setup"
├─ [45] glPushDebugGroup "Skybox Pass"
│   ├─ [46] glUseProgram
│   ├─ [47] glDrawArrays
│   └─ [48] glPopDebugGroup
├─ [89] glPushDebugGroup "PBR Pass"
│   ├─ ...
```

Les étiquettes de débogage rendent la navigation immédiate (voir [debugging.md](./debugging.md)).

## Lancement rapide via Justfile

Construction et lancement directement depuis la racine du projet :

```bash
# Build Debug + lancer RenderDoc
just renderdoc

# Surcharger le chemin du binaire RenderDoc
just renderdoc_bin=/chemin/vers/qrenderdoc renderdoc
```

## Instrumentation active (GL_KHR_debug)

Le moteur utilise `GL_KHR_debug` pour nommer les ressources et segmenter les commandes GPU pour RenderDoc.

### Naming des ressources (glObjectLabel)

Objets labellisés visibles dans le Resource Inspector :

| Catégorie | Exemples |
|-----------|----------|
| **Shader Programs** | `skybox.vert + skybox.frag`, `pbr_instanced.vert + pbr.frag` |
| **Buffers** | `Quad VBO`, `Wire Cube VBO`, `Wire Quad VBO` |
| **VAOs** | `Empty VAO`, `Fullscreen Quad VAO` |
| **Textures** | `Scene Color (HDR)`, `Velocity Buffer`, `Scene Depth (D32F_S8)` |
| **Texture Views** | `Scene Stencil View` |

### Groupes de debug (Event Browser RenderDoc)

La frame est structurée par des paires hiérarchiques `glPushDebugGroup`/`glPopDebugGroup` :

```text
Render_Frame
├─ Scene_Render
│   ├─ Skybox_Pass
│   ├─ Billboard_Sort_And_Render  (mode billboard transparent)
│   │   └─ tri + draw calls
│   └─ Instanced_Geometry_Render  (mode instancié)
│       └─ draw calls
├─ Post_Processing
│   ├─ PostFX_Bloom
│   ├─ PostFX_DepthOfField
│   ├─ PostFX_AutoExposure
│   ├─ PostFX_MotionBlur_Compute
│   └─ PostFX_Final_Composite
└─ UI_Overlay
```

Cette décomposition permet d'isoler le coût de chaque passe de rendu directement dans l'Event Browser.

### Ajout de nouveaux groupes de debug

Utiliser les fonctions helper de `gl_debug.h` :

```c
#include "gl_debug.h"

gl_debug_push_group("Mon_Pass_Custom");
/* ... commandes OpenGL ... */
gl_debug_pop_group();
```

Chaque `gl_debug_push_group()` doit être apparié avec un `gl_debug_pop_group()`.
Convention de nommage : `Feature_ObjectType` (ex. `PostFX_Bloom`, `Scene_Render`).

## Ressources textures et tampons

L'inspecteur de ressources permet de visualiser :
- Les textures à chaque étape (avant/après bloom, avant/après tonemapping)
- L'état des tampons (UBO, SSBO, VBO)
- Les framebuffers avec leur contenu

## Voir aussi

- [debugging.md](./debugging.md) — Étiquettes de débogage GL
- [gpu_profiling.md](./gpu_profiling.md) — Profilage GPU intégré
- [profiling_guide.md](./profiling_guide.md) — ApiTrace pour l'analyse automatisée
