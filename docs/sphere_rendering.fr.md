# Rendu des sphères

Ce document décrit l'architecture de rendu des sphères PBR avec tri Back-to-Front et anti-aliasing analytique.

La structure `BillboardSorter` gère le tri des sphères (CPU/GPU) pour le rendu correct de la transparence.

```c
typedef struct {
    BillboardSortEntry* entries;      // Scratchpad de tri CPU (index & depth)
    BillboardSortEntry* entries_aux;  // Tampon auxiliaire pour le tri Radix
    SphereInstance* temp_instances;   // Tampon CPU temporaire pour les instances réordonnées
    int min_capacity;
    int cpu_capacity;
    int ssbo_capacity;

    GLuint compute_program;           // Shader compute pour le tri bitonique GPU
    GLuint instance_ssbo;             // SSBO contenant les instances triées ou brutes
    GLuint indices_ssbo;              // SSBO d'indices pour le tri GPU
    GLuint temp_ssbo;                 // SSBO temporaire pour le tri bitonique
} BillboardSorter;
```

Pour le rendu correct des sphères semi-transparentes (Algorithme du Peintre) :

1. Calculer la distance au carré par rapport à la caméra pour chaque sphère
2. Trier les sphères par profondeur décroissante (arrière → avant)
3. Rendre dans l'ordre trié via les SSBOs ou VBOs

```c
// Tri via BillboardSorter (CPU Radix, CPU QuickSort ou GPU Bitonic Compute)
billboard_sorter_sort(sorter, instances, count, camera_pos, sorting_mode);
```

## Anti-aliasing analytique via discriminant

L'intersection d'un rayon avec une sphère peut être détectée analytiquement via le discriminant de l'équation quadratique. Pour les sphères, cela permet un AA sub-pixel sans MSAA :

```glsl
// Dans le fragment shader — rendu en billboard
// Coordonnée locale du fragment par rapport au centre du billboard
vec2 p = v_local_uv * 2.0 - 1.0; // [-1, 1]

// Distance au bord de la sphère (cercle en 2D)
float d = dot(p, p); // d = x² + y²

// AA analytique : smooth step dans la région de transition (rayon ~1.0)
float alpha = 1.0 - smoothstep(0.95, 1.05, d);

if (alpha < 0.01) discard;
```

### Lissage via discriminant

Pour simuler l'intersection correcte avec la sphère 3D :

```glsl
// Calcul de l'intersection rayon-sphère en espace local
float discriminant = 1.0 - d; // Pour une sphère unitaire centrée à l'origine

// AA : transition douce aux bords
float aa_width = fwidth(discriminant) * AA_SCALE;
float coverage = smoothstep(-aa_width, aa_width, discriminant);
```

Le rendu et l'upload GPU des données triées sont encapsulés dans le composant `BillboardRenderer` via la fonction `billboard_renderer_draw`, qui utilise les paramètres découplés `BillboardRenderParams` :

```c
// Rendu instancié via BillboardRenderer
billboard_renderer_draw(&renderer, profiler, &params);
```

## Interaction avec le frustum culling

Avant le rendu, les sphères en dehors du frustum sont éliminées via les AABB calculées dans l'espace NDC (voir [billboard_optimization.md](./billboard_optimization.md)) :

```
Total sphères : 512
Après frustum culling : ~200 (61 % éliminées en vue typique)
Draw call final : 1 (instancié, 200 instances)
```

## Voir aussi

- [gpu_sorting.md](./gpu_sorting.md) — Algorithmes de tri (Radix, Bitonique)
- [billboard_optimization.md](./billboard_optimization.md) — AABB exacte pour le culling
- [specular_aa.md](./specular_aa.md) — Anti-aliasing spéculaire
