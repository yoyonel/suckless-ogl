# Rendu des sphères

Ce document décrit l'architecture de rendu des sphères PBR avec tri Back-to-Front et anti-aliasing analytique.

## Architecture : BillboardSorter

La classe `BillboardSorter` gère le tri des sphères pour le rendu correct de la transparence.

```c
typedef struct {
    SphereInstance* instances;    // Données des instances (position, matériau)
    float* depths;                // Profondeurs pré-calculées
    uint32_t* sorted_indices;     // Indices triés (Back-to-Front)
    uint32_t count;
} BillboardSorter;
```

## Tri Back-to-Front

Pour le rendu correct des sphères semi-transparentes (Algorithme du Peintre) :

1. Calculer la profondeur dans l'espace caméra pour chaque sphère
2. Trier les sphères par profondeur décroissante (arrière → avant)
3. Rendre dans l'ordre trié

```c
// Calcul des profondeurs
for (uint32_t i = 0; i < sorter->count; i++) {
    vec3 pos_view = mat4_transform_point(view_matrix, sorter->instances[i].position);
    sorter->depths[i] = -pos_view.z; // Négatif car l'axe Z est tourné en espace vue
}

// Tri (voir gpu_sorting.md pour les algorithmes)
billboard_sorter_sort(sorter, sort_backend);
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

## Rendu instancié

Les sphères sont rendues en une seule passe via `glDrawArraysInstanced` :

```c
// Mettre à jour le VBO des instances avec les données triées
glBindBuffer(GL_ARRAY_BUFFER, icosphere_vbo);
glBufferSubData(GL_ARRAY_BUFFER, 0,
                sorter->count * sizeof(SphereInstance),
                sorter->sorted_instances);

// Un seul draw call pour toutes les sphères
glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, sorter->count);
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
