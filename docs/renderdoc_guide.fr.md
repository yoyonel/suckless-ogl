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

## 8. Débogage des shaders (compatibilité SPIR-V)

Le debugger de shaders de RenderDoc fonctionne en convertissant les sources GLSL capturées en SPIR-V via son compilateur interne `glslang`. SPIR-V impose des exigences plus strictes que les pilotes OpenGL natifs, qui assignent automatiquement les locations et bindings. Sans qualificateurs explicites, RenderDoc échoue silencieusement à compiler le shader et désactive le bouton **Debug**.

### 8.1 Prérequis pour le debug shader

Tous les shaders du projet sont désormais pleinement conformes SPIR-V. Les trois catégories de qualificateurs requis :

| Qualificateur | S'applique à | Exemple |
|---------------|-------------|---------|
| `layout(location = N)` | Tous les varyings inter-stages (`in`/`out`) | `layout(location = 0) out vec3 WorldPos;` |
| `layout(location = N)` | Tous les uniforms non-opaques (`mat4`, `vec3`, `int`, `float`, `bool`) | `layout(location = 0) uniform mat4 projection;` |
| `layout(binding = N)` | Tous les uniforms opaques (samplers, images, UBOs, SSBOs) | `layout(binding = 0) uniform sampler2D irradianceMap;` |

!!! warning "mat4 occupe 4 locations consécutives"
    Un `mat4` utilise 4 locations (une par vecteur colonne). Après `layout(location = 0) uniform mat4 projection;`, la prochaine location disponible est **4**.

### 8.2 Limitation `gl_DepthRange`

Le backend SPIR-V de `glslang` ne supporte pas `gl_DepthRange`. Puisque `suckless-ogl` n'appelle jamais `glDepthRange()` (utilisant la plage par défaut `[0, 1]`), l'expression :

```glsl
// Avant (échoue en SPIR-V)
gl_FragDepth = (gl_DepthRange.diff * ndcDepth + gl_DepthRange.near + gl_DepthRange.far) * 0.5;

// Après (équivalent pour la plage par défaut [0, 1])
gl_FragDepth = (ndcDepth + 1.0) * 0.5;
```

### 8.3 Validation avec glslangValidator

Pour vérifier qu'une paire de shaders compile en SPIR-V :

```bash
glslangValidator --target-env opengl shader.vert shader.frag
```

Le projet inclut un mode de lint strict qui valide tous les shaders :

```bash
just lint-shaders-strict
```

### 8.4 Carte des locations d'uniforms (PBR Billboard/Instanced)

Correspondance de référence pour les shaders PBR principaux (billboard et instanced partagent le même layout) :

| Location | Uniform | Type |
|----------|---------|------|
| 0 | `projection` | mat4 |
| 4 | `view` | mat4 |
| 8 | `previousViewProj` | mat4 |
| 12 | `camPos` | vec3 |
| 13 | `debugMode` | int |
| 14 | `u_screenSize` | vec2 |
| 15–21 | Uniforms SH probe | (via `sh_probe.glsl`) |

| Binding | Sampler | Type |
|---------|---------|------|
| 0 | `irradianceMap` | sampler2D |
| 1 | `prefilterMap` | sampler2D |
| 2 | `brdfLUT` | sampler2D |
| 3 | `ProbeBuffer` | SSBO |
| 8–14 | `u_SHTexture0`–`6` | sampler3D |

## Ressources textures et tampons

L'inspecteur de ressources permet de visualiser :
- Les textures à chaque étape (avant/après bloom, avant/après tonemapping)
- L'état des tampons (UBO, SSBO, VBO)
- Les framebuffers avec leur contenu

## Voir aussi

- [debugging.md](./debugging.md) — Étiquettes de débogage GL
- [gpu_profiling.md](./gpu_profiling.md) — Profilage GPU intégré
- [profiling_guide.md](./profiling_guide.md) — ApiTrace pour l'analyse automatisée
