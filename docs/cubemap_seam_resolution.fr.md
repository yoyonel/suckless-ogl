# Résolution des coutures de cubemap

Ce document décrit le problème des coutures visibles sur les cubemaps et la solution adoptée.

## Problème

Les cubemaps présentent des coutures visibles aux jointures entre les faces lors de l'application du filtre de la carte d'irradiance ou des niveaux de mip spéculaires. Ces artefacts se manifestent comme des lignes de couleur légèrement différente aux bords des faces.

## Cause

Les coutures apparaissent car :
1. L'interpolation bilinéaire ne traverse pas naturellement les frontières entre faces
2. Les calculs de dérivées (`dFdx`, `dFdy`) s'arrêtent aux bords de chaque face
3. Les échantillons aux coins partagés par trois faces reçoivent des valeurs différentes selon la face interrogée

## Solution : Images equirectangulaires

Plutôt que d'utiliser un cubemap pour stocker l'environnement, nous utilisons une **texture equirectangulaire** (panoramique cylindrical 360°). Cette projection élimine entièrement le problème de coutures car il n'y a qu'une seule texture continue.

### Conversion de la projection sphérique

```glsl
// Direction vers UV equirectangulaire
vec2 dir_to_uv(vec3 dir) {
    float phi = atan(dir.z, dir.x);       // -π à π
    float theta = asin(clamp(dir.y, -1.0, 1.0)); // -π/2 à π/2
    return vec2(phi / (2.0 * PI) + 0.5, theta / PI + 0.5);
}
```

### Avantages

| Critère | Cubemap | Equirectangulaire |
|---------|---------|------------------|
| Coutures | ❌ Aux 12 arêtes | ✅ Aucune |
| Filtrage | ❌ Complexe aux bords | ✅ Simple bilinéaire |
| Stockage | 6 textures | 1 texture |
| Compatibilité HDR | ✅ | ✅ |
| Coût mémoire (512px) | 6 × 512² = 1.57 Mi | 1024 × 512 = 0.5 Mi |

### Limitation

La singularité aux pôles (top et bottom) peut causer une légère distorsion dans les reflets très spéculaires pointés exactement vers le zénith ou le nadir. Dans la pratique, cet artefact est imperceptible.

## Voir aussi

- [equirectangular_seam_fix.md](./equirectangular_seam_fix.md) — Correctif de couture en mode répétition
- [skybox_rendering.md](./skybox_rendering.md) — Rendu de la skybox equirectangulaire
