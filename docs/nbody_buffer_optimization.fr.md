# Optimisation des uploads de buffers N-Body

## Problème constaté

Pendant l'animation N-body (`Shift+G`), l'application présente :

1. **Usage CPU élevé** sur le thread principal
2. **Sous-utilisation du GPU** — le GPU idle pendant que le CPU attend une synchronisation

La cause racine est une **synchronisation implicite CPU↔GPU** causée par
des appels `glBufferSubData` sur des buffers encore utilisés par le GPU
pour les draw calls du frame précédent.

## Contexte : synchronisation des buffers OpenGL

Quand `glBufferSubData` est appelé sur un buffer que le GPU lit encore
(pour un draw call soumis précédemment), le driver OpenGL a deux options :

1. **Bloquer le CPU** jusqu'à ce que le GPU ait fini de lire (sync implicite / stall)
2. **Allouer un nouveau backing store** et laisser le GPU finir de lire l'ancien

L'option 1 est le comportement par défaut de `glBufferSubData` — elle crée
une **bulle de pipeline** où ni le CPU ni le GPU ne sont productifs.

### Buffer Orphaning

Le **buffer orphaning** est une technique bien connue où l'application appelle :

```c
glBufferData(target, size, NULL, usage);   // orphan : allouer nouveau store
glBufferSubData(target, 0, size, data);    // écrire dans le nouveau store
```

L'appel `glBufferData(..., NULL, ...)` dit au driver : « je n'ai plus besoin
des anciennes données. » Le driver peut alors :

- Laisser le GPU continuer de lire l'ancien backing store
- Donner immédiatement au CPU un nouveau backing store (ou un recyclé)
- **Aucune synchronisation nécessaire** — CPU et GPU travaillent en parallèle

Ceci est documenté dans le
[Wiki OpenGL — Buffer Object Streaming](https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming)
et constitue l'approche standard pour les mises à jour dynamiques de buffers.

## Chemins de code affectés

### 1. VBO d'instances (`instanced_group_update`)

**Fichier** : `src/instanced_rendering.c`

Appelé une fois par frame depuis `scene_nbody_update()` pour uploader les 14
structures `SphereInstance` (matrices modèle, matériaux PBR, prev_position)
après l'intégration physique.

**Avant** (synchrone) :

```c
void instanced_group_update(InstancedGroup* group, const SphereInstance* data,
                            int count)
{
    group->instance_count = count;
    glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)(count * sizeof(SphereInstance)), data);
}
```

**Après** (avec orphaning) :

```c
void instanced_group_update(InstancedGroup* group, const SphereInstance* data,
                            int count)
{
    group->instance_count = count;
    GLsizeiptr size = (GLsizeiptr)(count * sizeof(SphereInstance));
    glBindBuffer(GL_ARRAY_BUFFER, group->instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}
```

**Taille des données** : 14 instances × 128 octets = **1 792 octets** par frame.

### 2. VBO des traînées (`trail_renderer_draw`)

**Fichier** : `src/trail_renderer.c`

Appelé une fois par frame pendant le rendu de la scène pour uploader la
géométrie ribbon face-caméra construite sur le CPU. Le staging buffer est
reconstruit chaque frame car la caméra bouge.

**Avant** (synchrone) :

```c
glBindBuffer(GL_ARRAY_BUFFER, trail->vbo);
glBufferSubData(GL_ARRAY_BUFFER, 0,
                (GLsizeiptr)(total_verts * sizeof(TrailVertex)),
                staging);
```

**Après** (avec orphaning) :

```c
glBindBuffer(GL_ARRAY_BUFFER, trail->vbo);
glBufferData(GL_ARRAY_BUFFER,
             (GLsizeiptr)(MAX_TRAIL_VERTICES * sizeof(TrailVertex)),
             NULL, GL_STREAM_DRAW);
glBufferSubData(GL_ARRAY_BUFFER, 0,
                (GLsizeiptr)(total_verts * sizeof(TrailVertex)),
                staging);
```

**Taille des données** : jusqu'à 32 corps × 514 vertices × 32 octets = **~527 Ko**
dans le pire cas (typiquement bien moins avec 14 corps et des traînées partielles).

## Pattern existant

La fonction `billboard_group_update()` dans `src/billboard_rendering.c`
utilise déjà exactement ce pattern d'orphaning :

```c
glBufferData(GL_ARRAY_BUFFER,
             (GLsizeiptr)(group->capacity * sizeof(SphereInstance)),
             NULL, GL_DYNAMIC_DRAW);
glBufferSubData(GL_ARRAY_BUFFER, 0,
                (GLsizeiptr)(count * sizeof(SphereInstance)), data);
```

Cette optimisation aligne les buffers d'instances et de traînées avec la
stratégie existante du buffer billboard.

## Mesure programmatique

Un test benchmark (`tests/test_benchmark_buffer_upload.c`) mesure le
coût d'upload par frame en utilisant l'API réelle de l'application. Il
exerce les vrais chemins de code `instanced_group_update` et
`trail_renderer_draw` avec une simulation NBody complète, mesure le timing
wall-clock avec `clock_gettime(CLOCK_MONOTONIC)`, et valide l'intégrité
des données via readback GPU.

Pour les détails complets sur l'architecture du benchmark, les paramètres,
les résultats de la baseline et les instructions d'utilisation, voir la
documentation dédiée :
[Benchmark d'Upload de Buffers NBody](nbody_benchmark.fr.md).

## Impact attendu

| Métrique | Avant | Après (attendu) |
|----------|-------|------------------|
| Upload VBO instances | Bloqué jusqu'à fin GPU | Immédiat (orphan + écriture) |
| Upload VBO traînées | Bloqué jusqu'à fin GPU | Immédiat (orphan + écriture) |
| Utilisation GPU | Chute pendant les stalls CPU | Soutenue — pas de bulles |
| Temps CPU par frame | Inclut l'attente de sync implicite | Calcul pur + memcpy uniquement |

L'amélioration est plus visible quand :

- La charge GPU est non-triviale (post-processing, IBL, subdiv élevé)
- Le framerate est élevé (vsync off → plus de frames → plus de stalls/s)
- Le buffer de traînées est large (beaucoup de corps, longues traînées)

## Travaux futurs

| Optimisation | Description | Complexité |
|-------------|-------------|------------|
| VBOs double-bufferés | Ping-pong entre 2 VBOs par frame | Moyenne |
| `glMapBufferRange` persistant | Map une fois, écriture chaque frame avec fences | Moyenne |
| Compute shader ribbons | Construire la géométrie des traînées sur GPU, supprimer le staging CPU | Élevée |

## Références

- [Wiki OpenGL — Buffer Object Streaming](https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming)
- [NVIDIA — OpenGL Performance Guide](https://developer.nvidia.com/opengl-performance)
- [docs/gpu-rendering-synchronization.md](gpu-rendering-synchronization.md) — Docs sync GPU du projet
- [docs/nbody_physics.md](nbody_physics.md) — Référence physique N-Body
- [docs/profiling_tracy.md](profiling_tracy.md) — Zones de profilage Tracy
