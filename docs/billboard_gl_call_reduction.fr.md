# Passe Billboard — Réduction des Appels GL

## Contexte

La passe `Billboard_Sort_And_Render`, observée dans RenderDoc, émet **~65 commandes GL**
avant et incluant l'appel `glDrawArraysInstanced`. La majorité sont de la configuration
de pipeline (uniforms, binds de textures, modes de blend, copies de buffers) qui peut être
drastiquement réduite.

Ce document suit le plan d'optimisation par paliers et son état d'implémentation.

## Décomposition Actuelle (~65 appels)

| Phase                  | Appels | Détail                                                                        |
| :--------------------- | :----- | :---------------------------------------------------------------------------- |
| Compute sort           | 12     | `bufferSubData`, `useProgram`, 3 uniforms, 3 SSBO binds, dispatch, barrier      |
| Copie buffer SSBO→VBO  | 7      | bind source/dest, copie, 2× unbind défensifs                                   |
| État blend             | 3      | `glEnablei`, `glBlendFunc`, `glDisablei`                                       |
| `glUseProgram`         | 1      | `pbr_ibl_billboard`                                                            |
| Textures IBL           | 6      | 3× (`glActiveTexture` + `glBindTexture`)                                       |
| Uniforms samplers      | 3      | **Redondant** — `layout(binding=0/1/2)` déjà défini dans le shader             |
| Uniforms par frame     | ~12    | projection, view, prevVP, camPos, screenSize, debugMode, params GI             |
| Textures SH (GI)       | 14     | 7× (`glActiveTexture` + `glBindTexture3D`)                                     |
| SSBO probe             | 1      | `glBindBufferBase`                                                             |
| VAO + draw             | 3      | `glBindVertexArray`, `glDisable(GL_CULL_FACE)`, `glDrawArraysInstanced`        |
| Nettoyage              | ~3     | unbind VAO, restaurer cull, désactiver blend                                  |

## Paliers d'Optimisation

### Palier 1 — Trivial, Aucun Changement Shader (~5 appels économisés)

**Statut : Terminé** ✅

| Optimisation | Appels économisés | Risque |
|-------------|-------------------|--------|
| Supprimer 3× `glUniform1i` pour les samplers (déjà `layout(binding=X)` en GLSL) | 3 | Aucun |
| Supprimer 2× unbind défensifs après `glCopyBufferSubData` | 2 | Aucun |

Total : **5 appels économisés**. Validé dans RenderDoc : 64 → 59 commandes.

### Palier 2 — AZDO Persistent Mapping pour le Billboard UBO

**Statut : Terminé** ✅ (Amélioré depuis `glBufferSubData`)

Initialement, nous avons remplacé ~12 appels `glUniform*` individuels par un seul UBO. Nous avons depuis amélioré cela vers du **AZDO Persistent Mapping**. L'UBO est désormais alloué avec `glBufferStorage` et mappé dans la mémoire CPU une seule fois. Les mises à jour se font via un simple `memcpy`.

**Côté GLSL** — nouveau fichier partagé `shaders/billboard_ubo.glsl` :

```glsl
layout(std140, binding = 1) uniform BillboardBlock {
    mat4 projection;
    mat4 view;
    mat4 previousViewProj;
    vec3 camPos;      int debugMode;
    vec2 u_screenSize; vec2 _bb_pad0;
    vec3 u_ProbeGridMin; int u_GIMode;
    vec3 u_ProbeGridMax; int u_specularAAEnabled;
    ivec3 u_ProbeGridDim; int u_aaMode;
    vec3 u_GridToIdxScale; float _bb_pad1;
};
```

**Côté C** — struct `BillboardUBO` dans `include/scene.h`, alignée `std140`.

**Garde conditionnelle** dans `shaders/sh_probe.glsl` — les déclarations individuelles
d'uniforms sont protégées par `#ifndef HAS_BILLBOARD_UBO` pour que le pipeline instancié
(qui n'utilise PAS l'UBO) continue à fonctionner.

#### Sécurité d'Alignement UBO

`glm_mat4_copy` de cglm utilise AVX `_mm256_store_ps` (alignement 32 octets requis).
API générique dans `include/gl_common.h` :

- `GL_UBO_ALIGNED` — attribut `__attribute__((aligned(32)))` pour typedef
- `GL_ASSERT_UBO_ALIGNMENT(type)` — `_Static_assert` compile-time

Appliqué à `BillboardUBO` et `PostProcessUBO`.

### Palier 3 — Bindings Persistants SH/IBL Textures & SSBO (~21 appels économisés)

**Statut : Terminé** ✅

| Optimisation | Appels économisés | Statut |
|-------------|-------------------|--------|
| Bind textures IBL une seule fois (Units 15-17) | 6 | **Terminé** |
| Bind textures 3D SH une seule fois (Units 8-14) | 14 | **Terminé** |
| Bind SSBO probe une seule fois (Binding 3) | 1 | **Terminé** |

**Stratégie de Cache IBL :**
En déplaçant les samplers IBL vers des unités hautes dédiées (15, 16, 17), nous garantissons qu'ils ne sont pas écrasés par les passes Skybox ou PostProcess. Nous utilisons désormais un cache de binding dans la structure `Scene` pour éliminer tous les appels `glActiveTexture` et `glBindTexture` par frame pour le rendu PBR.

### Palier 4 — Lecture Directe SSBO dans le Vertex Shader (~3 appels économisés)

**Statut : Terminé** ✅

Élimine la copie `glCopyBufferSubData` SSBO→VBO en lisant les instances triées
directement via `gl_InstanceID` depuis le SSBO dans le vertex shader.

**Insight clé :** le tri GPU binde déjà `sorted_instance_ssbo` au point de binding 2
via `glBindBufferBase`. Après le compute shader, `glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0)`
ne débinde que la cible générique — PAS le point de binding indexé. Donc binding 2 reste valide.

Pour les chemins CPU sort (qsort, radix), un seul `glBindBufferBase(binding 2, instance_ssbo)`
est ajouté à la fin de `upload_sorted_to_ssbo()` pour correspondre à la convention du tri GPU.

**Côté GLSL** — nouvel include partagé `shaders/billboard_instance_ssbo.glsl` :

```glsl
struct SphereInstance {
    mat4 model;
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float padding;
    float _pad[9];  // Correspondre au stride C 128 octets (SIMD_ALIGNMENT=64)
};

layout(std430, binding = 2) readonly buffer BillboardInstanceSSBO {
    SphereInstance billboard_instances[];
};
```

**Vertex shader** — `shaders/pbr_ibl_billboard.vert` récupère les données par instance
via `gl_InstanceID` au lieu d'attributs de vertex instantiés :

```glsl
// Avant (attributs vertex) :
layout(location = 2) in mat4 i_model;
layout(location = 6) in vec3 i_albedo;
layout(location = 7) in vec3 i_pbr;

// Après (fetch SSBO) :
SphereInstance inst = billboard_instances[gl_InstanceID];
float scaleX = length(vec3(inst.model[0]));
Albedo = inst.albedo;
Metallic = inst.metallic;  // Accès direct, pas de packing vec3
```

**Côté C** — Dans `src/scene.c`, l'appel `billboard_group_update_from_buffer()` est supprimé
entièrement (conservé uniquement pour le wireframe debug). Aucun `glBindBufferBase` supplémentaire
nécessaire dans le chemin GPU sort — binding 2 est déjà établi par le dispatch compute.

La copie VBO legacy est conservée uniquement pour l'overlay wireframe debug.

**Appels économisés :**

| Mode de tri          | Supprimés                        | Ajoutés                          | Net    |
| :------------------- | :------------------------------- | :------------------------------- | :----- |
| GPU bitonic (défaut) | 3 (bind read, bind write, copy)  | 0                                | **-3** |
| CPU qsort/radix      | 3                                | 1 (`glBindBufferBase` dans sort) | **-2** |

## Résultats Projetés

| Palier                 | Effort | Appels économisés | Restants |
| :--------------------- | :----- | :---------------- | :------- |
| Base                   | —      | —                 | **~65**  |
| Palier 1               | Trivial| 5                 | ~60      |
| Palier 2 (UBO)         | Moyen  | 11                | ~49      |
| Palier 3 (SH/SSBO)     | Moyen  | 15                | **~34**  |
| Palier 4 (SSBO direct) | Moyen  | 3                 | ~31      |
| Palier 5 (Nettoyage)   | Faible | ~8                | **~23**  |

### Palier 5 — Suppression des Unbinds Défensifs (~8+ appels économisés)

**Statut : Terminé** ✅

Suppression de tous les appels redondants `glBindVertexArray(0)` et `glBindBuffer(..., 0)` des chemins chauds (hot paths). Dans un pipeline OpenGL moderne, l'état est simplement écrasé par le prochain bind, faisant de ces appels de "nettoyage" un gaspillage significatif de cycles CPU.

**Modules affectés :**
- Rendu Billboard & Instancié
- Rendu SSBO
- Post-process & Skybox
- UI & FX LUT Viz

## Analyse de Régression de Performance

### Compromis du Palier 4 : Input Assembler vs Lecture SSBO

Le Palier 4 remplace le chemin traditionnel **Input Assembler matériel** (attributs vertex
par instance via VBO + `glVertexAttribDivisor`) par une **lecture SSBO manuelle** dans le
vertex shader (`billboard_instances[gl_InstanceID]`).

C'est un compromis architectural délibéré :

| Aspect | Avant (VBO + IA) | Après (lecture SSBO) |
|--------|-------------------|----------------------|
| Chemin de données | Input Assembler matériel (fonction fixe) | Instruction `buffer load` manuelle dans le shader |
| Bande passante | L'IA peut pré-charger/mettre en cache les flux d'attributs | Lecture cohérente unique par invocation |
| Latence | Matériel dédié, potentiellement coût nul | Instruction ALU + hit cache L2 (typiquement) |
| Appels GL | `glBindBufferBase` + `glCopyBufferSubData` + setup VBO | 0 appel supplémentaire (binding 2 réutilisé du tri) |

### Pourquoi C'est Sans Risque

**1. Charge de travail dominée par le fragment shader.** Chaque sphère billboard exécute un
fragment shader PBR + IBL complet avec :

- 3 lookups de textures IBL (cubemap irradiance, env map pré-filtrée, BRDF LUT)
- 7 lookups de textures 3D SH probe (harmoniques sphériques)
- Évaluation BRDF Cook-Torrance (NDF GGX, géométrie Smith, Fresnel)
- Tone mapping + correction gamma

Le coût du fragment shader par pixel **écrase** toute différence de fetch au stade vertex.
Pour une frame typique 1920×1080 avec 10–100 sphères, le vertex shader s'exécute ~4–600 fois
(4–6 vertices × instances) tandis que le fragment shader s'exécute des millions de fois.

**2. Accès cache-friendly.** La lecture SSBO accède à `billboard_instances[gl_InstanceID]`
séquentiellement à travers les instances. Avec des structs alignées sur 128 octets
(correspondant aux lignes de cache), cela produit d'excellents taux de hit cache L2 —
comparables à ce que le matériel Input Assembler obtiendrait pour les mêmes données.

**3. Nombre d'instances négligeable.** Le système billboard rend 10–100 sphères. Même avec
une pénalité pessimiste de 10ns par invocation vertex (improbable), le surcoût total serait :

$$100 \text{ instances} \times 6 \text{ vertices} \times 10\text{ns} = 6\mu\text{s}$$

Soit **trois ordres de grandeur** en dessous d'un budget frame typique de 16ms.

**4. Réduction du surcoût driver.** Les 3 appels GL supprimés (bind + copie + unbind)
éliminent la validation côté CPU du driver et l'enregistrement du command buffer. Sur les
scènes lourdes en draw calls, cette économie CPU peut dépasser le coût GPU théorique des
lectures manuelles.

### Mesurer l'Impact

Le projet inclut un système `GPUProfiler` ([src/gpu_profiler.c](gpu_profiler.c)) avec des
timestamp queries par stage, mais la passe Billboard manque actuellement d'un stage de
profiling dédié. Pour mesurer l'impact réel :

1. **Ajouter `GPU_STAGE_PROFILER`** autour du bloc billboard sort+render dans `scene.c`
2. **Utiliser le pattern `EffectBenchmark` existant** ([src/effect_benchmark.c](effect_benchmark.c)) :
   warmup 30 frames, mesure 120 frames, rapport mean ± stddev
3. **Comparer les branches** en exécutant le même point de vue caméra sur `master` vs `refactor/`
4. **Résultat attendu** : delta dans le bruit de mesure (< 1% variation), confirmant la
   dominance fragment-bound

Un futur mode CLI `--benchmark N` pourrait automatiser cette comparaison entre branches.

### Conclusion

La lecture SSBO est un **compromis net positif** : coût GPU négligeable (s'il existe) en
échange de 3 appels GL en moins, élimination de la copie VBO, et un flux de données plus
simple où la sortie du tri est consommée directement par le vertex shader sans copies
intermédiaires.

## Fichiers Concernés

| Fichier | Rôle |
|---------|------|
| `src/scene.c` | `scene_render_billboards()` — upload UBO, bind textures, bind SSBO |
| `src/billboard_rendering.c` | `billboard_group_update_from_buffer()` — copie VBO legacy (debug uniquement) |
| `src/billboard_rendering.c` | `billboard_group_draw()` — bind VAO, état cull, draw call |
| `src/sphere_sorting.c` | `sphere_sorter_sort_gpu()` — dispatch compute |
| `shaders/billboard_instance_ssbo.glsl` | **Nouveau** — SSBO SphereInstance pour lecture directe vertex (`binding = 2`) |
| `shaders/billboard_ubo.glsl` | Définition bloc UBO partagé (`binding = 1`) |
| `shaders/pbr_ibl_billboard.vert` | Vertex shader — fetch SSBO via `gl_InstanceID` |
| `shaders/pbr_ibl_billboard.frag` | Fragment shader — inclut `billboard_ubo.glsl` |
| `shaders/sh_probe.glsl` | Uniforms SH probe — gardé par `#ifndef HAS_BILLBOARD_UBO` |
| `include/scene.h` | `BillboardUBO` struct + `BillboardUniforms` (samplers SH uniquement) |
| `include/gl_common.h` | API générique `GL_UBO_ALIGNED` / `GL_ASSERT_UBO_ALIGNMENT` |
| `include/postprocess.h` | `PostProcessUBO` — garde d'alignement appliquée |
