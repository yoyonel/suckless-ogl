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

**Statut : En cours**

| Optimisation | Appels économisés | Risque |
|-------------|-------------------|--------|
| Supprimer 3× `glUniform1i` pour les samplers (déjà `layout(binding=X)` en GLSL) | 3 | Aucun |
| Supprimer 2× unbind défensifs après `glCopyBufferSubData` | 2 | Aucun |

Total : **~5 appels économisés** (sous-ensemble conservatif et sûr).

### Palier 2 — UBO pour les Uniforms Par Frame (~12 appels → 1)

**Statut : Planifié**

Remplacer les appels `glUniform*` individuels par un seul **Uniform Buffer Object**, suivant
le pattern `PostProcessUBO` existant dans `src/postprocess.c`.

```c
typedef struct {
    mat4 projection;        // offset 0
    mat4 view;              // offset 64
    mat4 previousViewProj;  // offset 128
    vec3 camPos;            // offset 192
    int  debugMode;         // offset 204
    vec2 screenSize;        // offset 208
    vec2 _pad0;             // offset 216 (alignement std140)
    vec3 probeGridMin;      // offset 224
    int  giMode;            // offset 236
    vec3 probeGridMax;      // offset 240
    int  specularAAEnabled; // offset 252
    ivec3 probeGridDim;     // offset 256
    int   aaMode;           // offset 268
} BillboardUBO;
```

Un seul `glBufferSubData` + `glBindBufferBase` remplace ~12 appels individuels.

### Palier 3 — Bindings Persistants de Textures/Buffers (~21 appels économisés)

**Statut : Planifié**

| Optimisation | Appels économisés |
|-------------|-------------------|
| Bind textures IBL une seule fois au chargement (pas par frame) | 6 |
| Bind textures 3D SH une seule fois quand la probe grid change | 14 |
| Bind SSBO probe une seule fois quand la probe grid change | 1 |

Nécessite un flag "dirty" sur les mises à jour de la probe grid pour re-bind uniquement au changement.

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
| Palier 3 (Persistant) | Moyen | 21 | **~28** |
| Palier 4 (SSBO direct) | Moyen-Haut | 7 | **~21** |

## Fichiers Concernés

| Fichier | Rôle |
|---------|------|
| `src/scene.c` | `scene_render_billboards()` — setup uniforms, bind textures |
| `src/billboard_rendering.c` | `billboard_group_update_from_buffer()` — copie SSBO→VBO |
| `src/billboard_rendering.c` | `billboard_group_draw()` — bind VAO, état cull, draw call |
| `src/sphere_sorting.c` | `sphere_sorter_sort_gpu()` — dispatch compute |
| `shaders/pbr_ibl_billboard.vert` | Vertex shader — `layout(binding)` explicite |
| `shaders/pbr_ibl_billboard.frag` | Fragment shader — `layout(binding=0/1/2)` pour IBL |
| `shaders/sh_probe.glsl` | Uniforms et bindings textures SH probe |
| `include/scene.h` | Définition struct `BillboardUniforms` |
