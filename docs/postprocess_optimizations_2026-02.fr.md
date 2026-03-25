# Post-Process Pipeline — Optimisations & Corrections (Février 2026)

> **Date** : 7 février 2026
> **Cible** : OpenGL 4.6, Mesa / Intel Iris Xe (RPL-U), iGPU mémoire unifiée
> **Fichiers modifiés** : 12 fichiers (headers, sources C, shaders, tests)

---

## Table des matières

1. [Résumé](#resume)
2. [Corrections de bugs](#1-corrections-de-bugs)
   - [FXAA / Motion Blur — Ordre du pipeline fragment](#11-fxaa-motion-blur-ordre-du-pipeline-fragment)
   - [Neighbor Max — Sur-dispatch du compute](#12-neighbor-max-sur-dispatch-du-compute)
   - [Luminance Adaptation — Réduction parallèle](#13-luminance-adaptation-reduction-parallele)
   - [Barrières mémoire manquantes](#14-barrieres-memoire-manquantes)
   - [Auto-Exposure — FBO unbind avant compute](#15-auto-exposure-fbo-unbind-avant-compute)
3. [Optimisations de performance](#2-optimisations-de-performance)
   - [UBO dirty flag — Upload partiel](#21-ubo-dirty-flag-upload-partiel)
   - [Sampler uniforms — Liaison unique](#22-sampler-uniforms-liaison-unique)
   - [VAO bind/unbind — Factorisation](#23-vao-bindunbind-factorisation)
4. [Refactoring du GPU Profiler](#3-refactoring-du-gpu-profiler)
   - [Double-buffering des métadonnées](#31-double-buffering-des-metadonnees)
   - [Séparation recording_count / stage_count](#32-separation-recording_count-stage_count)
   - [Étapes de profiling restructurées](#33-etapes-de-profiling-restructurees)
5. [Fichiers modifiés](#4-fichiers-modifies)
6. [Validation](#5-validation)

---

## Résumé

Série d'optimisations et corrections apportées au pipeline de post-processing
et au GPU profiler. Les changements couvrent quatre axes :

- **Corrections de bugs** dans l'ordre d'exécution des effets fragment, le
  dispatch des compute shaders, et les barrières de synchronisation.
- **Optimisations CPU-side** réduisant les appels OpenGL redondants par frame.
- **Refactoring du profiler GPU** pour corriger les conflits d'index entre
  frames dans le système double-buffered.
- **Restructuration du profiling** pour séparer le coût compute du coût
  fragment dans le motion blur.

---

## 1. Corrections de bugs

### 1.1 FXAA / Motion Blur — Ordre du pipeline fragment

**Fichier** : `shaders/postprocess.frag`

**Problème** : Le FXAA était appliqué directement sur `screenTexture` *avant*
le Motion Blur et le Chromatic Aberration. Le Motion Blur n'était donc jamais
lissé par le FXAA, et le FXAA ne bénéficiait pas du contenu motion-blurred.

**Avant** :
```
FXAA → (CA → MB) ou texture directe
```

**Après** :
```
(MB → CA) → FXAA → DoF → Bloom → ...
```

Le FXAA reçoit maintenant la couleur déjà traitée par MB+CA via le paramètre
`color`, au lieu de re-sampler `screenTexture` indépendamment.

### 1.2 Neighbor Max — Sur-dispatch du compute

**Fichier** : `src/effects/fx_motion_blur.c`

**Problème** : Le pass Neighbor Max dispatching était identique au Tile Max
(`groups = ceil(pixels / 16)`), alors que le Neighbor Max opère sur les
*tiles*, pas sur les pixels. Cela provoquait un sur-dispatch de 16× en X et
16× en Y (256× au total).

**Avant** :
```c
glDispatchCompute(groups_x, groups_y, 1);  // groups = ceil(width/16)
// Pour 1920×1080 : 120×68 = 8160 groupes (chacun 16×16 = 256 threads)
// Total: ~2M threads pour traiter 8160 tiles
```

**Après** :
```c
int neighbor_groups_x = (tile_count_x + 15) / 16;
int neighbor_groups_y = (tile_count_y + 15) / 16;
glDispatchCompute(neighbor_groups_x, neighbor_groups_y, 1);
// Pour 1920×1080 : 8×5 = 40 groupes → ~10K threads pour 8160 tiles
```

### 1.3 Luminance Adaptation — Réduction parallèle

**Fichier** : `shaders/lum_adapt.comp`

**Problème** : Le compute shader d'adaptation de luminance utilisait un seul
thread (`gl_GlobalInvocationID == (0,0)`) qui itérait séquentiellement sur les
64×64 = 4096 texels de la carte de luminance.

**Après** : Réduction parallèle en shared memory avec 256 threads (16×16),
chaque thread traitant 16 texels (bloc 4×4), suivie d'une réduction
logarithmique `256 → 128 → 64 → ... → 1`.

```glsl
layout(local_size_x = 16, local_size_y = 16) in;
shared float sharedLogLum[256];
shared float sharedValidCount[256];

// Chaque thread accumule 4×4 texels
// Réduction parallèle avec barrier()
for (uint s = 128u; s > 0u; s >>= 1u) { ... }
```

### 1.4 Barrières mémoire manquantes

**Fichier** : `src/postprocess.c`

**Ajout** : `glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT)` en début de
`postprocess_end()`, après le rendu de la scène MRT et avant les effets
compute (Bloom, DoF, AE, MB). Garantit que les écritures dans les textures
de couleur, vélocité et depth/stencil sont visibles aux compute shaders.

### 1.5 Auto-Exposure — FBO unbind avant compute

**Fichier** : `src/effects/fx_auto_exposure.c`

**Problème** : Le FBO de downsample restait bindé pendant le dispatch du
compute shader d'adaptation. Le compute shader lisait `downsample_tex` qui
était encore attachée au FBO actif, créant un conflit lecture/écriture
potentiel.

**Après** : Le FBO est unbindé (`glBindFramebuffer(GL_FRAMEBUFFER, 0)`)
*avant* la barrière mémoire et le dispatch, garantissant que les données
rasterisées sont flushées.

---

## 2. Optimisations de performance

### 2.1 UBO dirty flag — Upload partiel

**Fichiers** : `include/postprocess.h`, `src/postprocess.c`

**Problème** : Le `PostProcessUBO` (~300 octets, layout std140) était
reconstruit et uploadé en totalité chaque frame via `glBufferSubData`, même
si aucun paramètre n'avait changé (seul `time` change chaque frame).

**Solution** : Ajout d'un flag `bool ubo_dirty` dans la struct `PostProcess`.

- **Quand `ubo_dirty = true`** : Reconstruction complète du UBO et upload de
  `sizeof(PostProcessUBO)` octets.
- **Quand `ubo_dirty = false`** : Upload partiel de 8 octets uniquement
  (`active_effects` + `time`), les deux premiers champs du UBO.

Le flag est mis à `true` dans les 17 setters de paramètres (`postprocess_set_*`)
et à l'initialisation. `postprocess_update_time()` ne met **pas** le flag à
`true` car `time` est dans l'en-tête toujours mis à jour.

```c
if (post_processing->ubo_dirty) {
    PostProcessUBO ubo = { /* ... full rebuild ... */ };
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PostProcessUBO), &ubo);
    post_processing->ubo_dirty = false;
} else {
    struct { uint32_t active_effects; float time; } header = { ... };
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(header), &header);
}
```

**Gain** : En régime stationnaire (pas d'interaction UI), l'upload passe de
~300 octets/frame à 8 octets/frame.

### 2.2 Sampler uniforms — Liaison unique

**Fichiers** : `src/postprocess.c`, `src/effects/fx_bloom.c`,
`src/effects/fx_dof.c`, `src/effects/fx_auto_exposure.c`,
`src/effects/fx_motion_blur.c`

**Problème** : Les `shader_set_int(shader, "samplerName", unit)` étaient
appelés chaque frame dans les fonctions `*_render()`, alors que les sampler
uniforms sont un **état par programme** (`glProgramUniform`) qui ne change
jamais : chaque sampler est toujours associé à la même unité de texture.

**Solution** :

1. **Shader postprocess (uber-shader)** : Les 8 liaisons sampler→unité sont
   configurées dans `setup_sampler_uniforms()`, appelée une seule fois lors de
   `update_current_shader()` (changement ou recompilation du shader).

2. **Shaders d'effets** : Les sampler uniforms sont configurés dans les
   fonctions `*_init()` :
   - Bloom : `srcTexture = 0` sur prefilter, downsample, upsample
   - Auto-Exposure : `sceneTexture = 0` sur downsample, `lumTexture = 0` sur adapt
   - Motion Blur : `velocityTexture = 0` sur tile_max, `tileMaxTexture = 0`
     sur neighbor_max
   - DoF : Réutilise les shaders Bloom (déjà configurés)

**Gain** : Suppression de ~15 appels `glGetUniformLocation` + `glUniform1i`
par frame.

**Note** : `silent_warnings = true` est maintenant positionné *avant*
`update_current_shader()` dans `postprocess_compile_optimized()` pour éviter
les warnings de uniforms manquants sur les samplers compilés out.

### 2.3 VAO bind/unbind — Factorisation

**Fichiers** : `src/postprocess.c`, `src/effects/fx_bloom.c`,
`src/effects/fx_dof.c`, `src/effects/fx_auto_exposure.c`

**Problème** : Chaque effet (Bloom, DoF, AE downsample) bindait et unbindait
le VAO du screen quad indépendamment (`glBindVertexArray(vao)` /
`glBindVertexArray(0)`), générant des changements d'état inutiles vu que tous
les fullscreen passes utilisent le même VAO.

**Solution** : Le VAO est bindé **une seule fois** en début de
`postprocess_end()` et unbindé **une seule fois** après le dernier
`glDrawArrays` (Final Composite).

```c
// Début de postprocess_end()
glBindVertexArray(post_processing->screen_quad_vao);

// ... Bloom, DoF, AE, MB, Final Composite ...

// Après le dernier draw
glBindVertexArray(0);
```

**Gain** : Suppression de ~6 appels `glBindVertexArray` par frame (3 bind +
3 unbind dans Bloom, DoF, AE).

---

## 3. Refactoring du GPU Profiler

### 3.1 Double-buffering des métadonnées

**Fichiers** : `include/gpu_profiler.h`, `src/gpu_profiler.c`

**Problème** : Les métadonnées des stages (nom, couleur, depth, parent_index)
étaient écrites directement dans `profiler->stages[]` pendant l'enregistrement.
Ce tableau est aussi lu par l'UI pour l'affichage. Avec le double-buffering
des queries, les index de la frame N d'écriture pouvaient écraser les
métadonnées de la frame N-1 encore en lecture.

**Solution** : Ajout de `GPUStageInfo stage_info[MAX_GPU_STAGES]` dans chaque
`GPUQueryBuffer`. Les métadonnées sont écrites dans le buffer d'écriture
pendant `gpu_profiler_start_stage()`, puis restaurées dans `stages[]` lors du
read-back dans `gpu_profiler_begin_frame()`.

```c
typedef struct {
    char name[MAX_GPU_STAGE_NAME];
    uint32_t color;
    int depth;
    int parent_index;
} GPUStageInfo;
```

### 3.2 Séparation recording_count / stage_count

**Fichiers** : `include/gpu_profiler.h`, `src/gpu_profiler.c`,
`tests/test_gpu_profiler.c`

**Problème** : `stage_count` servait à la fois de compteur d'enregistrement
(write-path) et de compteur d'affichage (read-path). Après le swap de buffers,
il était remis à 0, ce qui faisait clignoter l'UI pendant une frame.

**Solution** : Deux compteurs distincts :
- `recording_count` : compteur d'écriture, remis à 0 à chaque
  `begin_frame()`.
- `stage_count` : compteur d'affichage, mis à jour depuis le read buffer
  complété, **jamais** remis à 0.

### 3.3 Étapes de profiling restructurées

**Fichiers** : `src/postprocess.c`, `include/app_settings.h`

Le profiling du post-process est restructuré pour distinguer :

| Stage              | Contenu                                        |
|--------------------|------------------------------------------------|
| **Post-Process**   | Englobant (parent de tous les sous-stages)      |
| Bloom              | Prefilter + Downsample + Upsample              |
| DoF                | Downsample + Tent blur                          |
| Auto-Exposure      | Lum downsample + Compute adaptation             |
| MB Compute         | Tile Max + Neighbor Max (compute dispatches)    |
| **Final Composite**| Fullscreen quad : MB sampling, CA, FXAA, etc.   |

Ajout de la couleur `GPU_PROFILER_COMPOSITE_COLOR` (Nord Frost Medium Blue,
`0x81A1C1`) dans `app_settings.h`.

---

## 4. Fichiers modifiés

| Fichier | Changements |
|---------|-------------|
| `include/postprocess.h` | Ajout `bool ubo_dirty` |
| `include/gpu_profiler.h` | `GPUStageInfo`, `recording_count`, docs |
| `include/perf_timer.h` | Clarification docs `GPUTimer` |
| `include/app_settings.h` | `GPU_PROFILER_COMPOSITE_COLOR` |
| `src/postprocess.c` | UBO dirty flag, `setup_sampler_uniforms()`, VAO factorisation, barrière mémoire, profiling restructuré |
| `src/gpu_profiler.c` | Double-buffering métadonnées, recording_count, docs |
| `src/effects/fx_bloom.c` | Sampler init, suppression VAO/sampler per-frame |
| `src/effects/fx_dof.c` | Suppression VAO/sampler per-frame |
| `src/effects/fx_auto_exposure.c` | Sampler init, FBO unbind, suppression VAO/sampler per-frame |
| `src/effects/fx_motion_blur.c` | Sampler init, dispatch corrigé, docs |
| `shaders/postprocess.frag` | Réordonnancement FXAA/MB/CA |
| `shaders/lum_adapt.comp` | Réduction parallèle 256 threads |
| `tests/test_gpu_profiler.c` | Adaptation aux nouveaux champs |

---

## 5. Validation

- **Build** : `make all` — 0 erreurs, 0 warnings
- **Tests** : `make test` — 32/32 tests passés (100%)
- **Lint** : `make lint` — Tous les checks passés (clang-tidy)
- **Exécution** : `make run-release` — Vérification visuelle OK
