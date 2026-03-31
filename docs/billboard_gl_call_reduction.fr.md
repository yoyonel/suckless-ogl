# Passe Billboard — Réduction des Appels GL

## Contexte

La passe `Billboard_Sort_And_Render`, observée dans RenderDoc, émet **~65 commandes GL**
avant et incluant l'appel `glDrawArraysInstanced`. La majorité sont de la configuration
de pipeline (uniforms, binds de textures, modes de blend, copies de buffers) qui peut être
drastiquement réduite.

Ce document suit le plan d'optimisation par paliers et son état d'implémentation.

## Décomposition Actuelle (~65 appels)

| Phase | Appels | Détail |
|-------|--------|--------|
| Compute sort | 12 | `bufferSubData`, `useProgram`, 3 uniforms, 3 SSBO binds, dispatch, barrier |
| Copie buffer SSBO→VBO | 7 | bind source/dest, copie, 2× unbind défensifs |
| État blend | 3 | `glEnablei`, `glBlendFunc`, `glDisablei` |
| `glUseProgram` | 1 | `pbr_ibl_billboard` |
| Textures IBL | 6 | 3× (`glActiveTexture` + `glBindTexture`) |
| Uniforms samplers | 3 | **Redondant** — `layout(binding=0/1/2)` déjà défini dans le shader |
| Uniforms par frame | ~12 | projection, view, prevVP, camPos, screenSize, debugMode, params GI |
| Textures SH (GI) | 14 | 7× (`glActiveTexture` + `glBindTexture3D`) |
| SSBO probe | 1 | `glBindBufferBase` |
| VAO + draw | 3 | `glBindVertexArray`, `glDisable(GL_CULL_FACE)`, `glDrawArraysInstanced` |
| Nettoyage | ~3 | unbind VAO, restaurer cull, désactiver blend |

## Paliers d'Optimisation

### Palier 1 — Trivial, Aucun Changement Shader (~5 appels économisés)

**Statut : Terminé** ✅

| Optimisation | Appels économisés | Risque |
|-------------|-------------------|--------|
| Supprimer 3× `glUniform1i` pour les samplers (déjà `layout(binding=X)` en GLSL) | 3 | Aucun |
| Supprimer 2× unbind défensifs après `glCopyBufferSubData` | 2 | Aucun |

Total : **5 appels économisés**. Validé dans RenderDoc : 64 → 59 commandes.

### Palier 2 — UBO pour les Uniforms Par Frame (~12 appels → 2)

**Statut : Terminé** ✅

Remplacement des ~12 appels `glUniform*` individuels par un seul upload **Uniform Buffer Object**
(`glBufferSubData` + `glBindBuffer`), suivant le pattern `PostProcessUBO` existant.

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

- `GL_UBO_ALIGNMENT` — constante (32)
- `GL_UBO_ALIGNED` — attribut `__attribute__((aligned(32)))` pour typedef
- `GL_ASSERT_UBO_ALIGNMENT(type)` — `_Static_assert` compile-time

Appliqué à `BillboardUBO` et `PostProcessUBO`.

### Palier 3 — Bindings Persistants SH Textures/SSBO (~15 appels économisés)

**Statut : En cours**

| Optimisation | Appels économisés | Cachable ? |
|-------------|-------------------|------------|
| ~~Bind textures IBL une seule fois~~ | ~~6~~ | **NON** — units 0-2 clobbées par Skybox et PostProcess |
| Bind textures 3D SH une seule fois quand probe grid change | 14 | **OUI** — units 8-14 exclusives aux passes PBR |
| Bind SSBO probe une seule fois quand probe grid change | 1 | **OUI** — binding 3 exclusif aux passes PBR |

**Pourquoi les textures IBL ne peuvent PAS être cachées :**
Dans un renderer multi-passe, les texture units 0-2 sont partagées :
- `src/skybox.c` re-bind `GL_TEXTURE0` avec la cubemap d'environnement
- `src/postprocess.c` re-bind les units 0-1-2 avec les FBO, bloom, etc.

Cacher ces bindings nécessiterait un tracker d'état GL centralisé (over-engineering).
Les textures SH (units 8-14) et le SSBO probe (binding 3) sont sûrs car aucune
autre passe ne touche ces units/bindings.

**Invalidation :** après `light_probe_grid_sync()` qui appelle `glBindTexture(GL_TEXTURE_3D, 0)`
sur l'unité active courante, pouvant écraser un binding SH caché.

### Palier 4 — Lecture Directe SSBO dans le Vertex Shader (~7 appels économisés)

**Statut : Planifié**

Éliminer la copie `glCopyBufferSubData` SSBO→VBO en lisant les instances triées
directement via `gl_InstanceID` depuis le SSBO dans le vertex shader.

```glsl
// Dans pbr_ibl_billboard.vert — remplacer les attributs par instance par un fetch SSBO
layout(std430, binding = 2) readonly buffer SortedInstances {
    SphereInstance instances[];
};
// ...
SphereInstance inst = instances[gl_InstanceID];
```

## Résultats Projetés

| Palier | Effort | Appels économisés | Restants |
|--------|--------|-------------------|----------|
| Base | — | — | **~65** |
| Palier 1 | Trivial | 5 | ~60 |
| Palier 2 (UBO) | Moyen | 11 | ~49 |
| Palier 3 (SH/SSBO) | Moyen | 15 | **~34** |
| Palier 4 (SSBO direct) | Moyen-Haut | 7 | **~27** |

## Fichiers Concernés

| Fichier | Rôle |
|---------|------|
| `src/scene.c` | `scene_render_billboards()` — upload UBO, bind textures |
| `src/billboard_rendering.c` | `billboard_group_update_from_buffer()` — copie SSBO→VBO |
| `src/billboard_rendering.c` | `billboard_group_draw()` — bind VAO, état cull, draw call |
| `src/sphere_sorting.c` | `sphere_sorter_sort_gpu()` — dispatch compute |
| `shaders/billboard_ubo.glsl` | **Nouveau** — définition bloc UBO partagé (`binding = 1`) |
| `shaders/pbr_ibl_billboard.vert` | Vertex shader — inclut `billboard_ubo.glsl` |
| `shaders/pbr_ibl_billboard.frag` | Fragment shader — inclut `billboard_ubo.glsl` |
| `shaders/sh_probe.glsl` | Uniforms SH probe — gardé par `#ifndef HAS_BILLBOARD_UBO` |
| `include/scene.h` | Struct `BillboardUBO` + `BillboardUniforms` (samplers SH uniquement) |
| `include/gl_common.h` | API générique `GL_UBO_ALIGNED` / `GL_ASSERT_UBO_ALIGNMENT` |
| `include/postprocess.h` | `PostProcessUBO` — garde d'alignement appliquée |
