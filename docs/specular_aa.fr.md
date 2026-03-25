# Anti-aliasing spéculaire (Specular AA)

Ce document décrit le filtrage de variance pour l'anti-aliasing spéculaire (Specular Anti-Aliasing).

## Problème : Aliasing spéculaire

Les matériaux à très faible rugosité (métaux polis, surfaces miroirs) présentent des highlights spéculaires très concentrées qui scintillent et s'aliasent aux hautes fréquences géométriques.

## Solution : Filtrage Varef (variance-based roughness filtering)

L'approche Varef augmente localement la rugosité en fonction de la variance de la normale dans la micro-géométrie, amortissant les highlights spéculaires aux zones aliasées.

### Formulation

```glsl
float compute_specular_aa_roughness(vec3 N, float roughness)
{
    // Calculer la variance de la normale (via les dérivées écran-espace)
    vec3 dNdx = dFdx(N);
    vec3 dNdy = dFdy(N);

    float variance = dot(dNdx, dNdx) + dot(dNdy, dNdy);

    // Augmenter la rugosité proportionnellement à la variance
    float kernel_roughness = min(2.0 * variance, SPECULAR_AA_THRESHOLD);
    float filtered_roughness = sqrt(roughness * roughness + kernel_roughness);

    return clamp(filtered_roughness, 0.0, 1.0);
}
```

## Modes d'activation

| Mode | Description | Activation |
|------|-------------|-----------|
| **Espace écran** | Basé sur `dFdx`/`dFdy` de la normale | Par défaut |
| **Courbure analytique** | Basé sur la courbure de la sphère | Sphères uniquement |
| **Désactivé** | Aucun filtrage spéculaire | `#undef ENABLE_SPECULAR_AA` |

## Mode courbure pour les sphères

Pour les sphères, la courbure est analytiquement connue, permettant un filtrage plus précis sans dépendance sur les dérivées :

```glsl
// Courbure d'une sphère de rayon r à une distance d
float curvature = 1.0 / (sphere_radius * sphere_radius);
float roughness_delta = SPECULAR_AA_SCALE * curvature;
float aa_roughness = sqrt(roughness * roughness + roughness_delta);
```

## Constantes de réglage

| Constante | Valeur par défaut | Effet |
|-----------|------------------|-------|
| `SPECULAR_AA_THRESHOLD` | `0.18` | Plafond de la correction de rugosité |
| `SPECULAR_AA_SCALE` | `8e-4` | Intensité du mode courbure |

Augmenter `SPECULAR_AA_THRESHOLD` réduit davantage le scintillement mais peut rendre les surfaces trop mates.

## Compatibilité cross-GPU

Le mode basé sur les dérivées (`dFdx`/`dFdy`) peut diverger sur NVIDIA (voir [gpu-rendering-synchronization.md](./gpu-rendering-synchronization.md)). Dans les cas problématiques, utiliser le mode courbure analytique ou désactiver Specular AA.

## Voir aussi

- [gpu-rendering-synchronization.md](./gpu-rendering-synchronization.md) — Problèmes de dérivées inter-fournisseurs
- [shader-cross-gpu-compatibility.md](./shader-cross-gpu-compatibility.md) — Directives cross-GPU
- [sphere_rendering.md](./sphere_rendering.md) — Rendu des sphères avec AA analytique
