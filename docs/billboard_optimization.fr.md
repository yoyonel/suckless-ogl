# Optimisation du rendu des billboards

Ce document décrit l'algorithme d'optimisation du calcul des AABB (boîtes englobantes alignées aux axes) pour les sphères billboards.

## Contexte

Pour le frustum culling des sphères, nous avons besoin d'une AABB précise dans l'espace de découpage NDC. L'approche naïve (projeter les 8 coins de la boîte englobante 3D) est inexacte pour les sphères near-plane ou hors-axe.

## AABB exacte par plans tangents

La méthode correcte projette les **plans tangents** de la sphère — les plans qui touchent la sphère exactement à un cercle depuis le point de vue de la caméra.

### Formulation mathématique

Pour une sphère de centre $\mathbf{c}$ et rayon $r$ :

1. La distance d'un rayon depuis l'origine est $d = \|\mathbf{c}\|$
2. Le demi-angle du cône de visibilité est $\sin\theta = r/d$
3. Les plans tangents sont perpendiculaires à la direction camera→sphère

### Projection NDC

Pour chaque axe (X et Y), on calcule le min/max des coordonnées NDC des points tangents :

```glsl
// Projection du point tangent sur l'axe X en NDC
float tangent_x = (c.x * cos_theta ± c.z * sin_theta) / abs_cos;
float ndc_x = tangent_x * proj_x;
```

## Gestion des cas limites

L'algorithme gère 6 cas spéciaux pour assurer la robustesse :

| Cas | Description | Solution |
|-----|-------------|----------|
| Sphère derrière la caméra | `d < 0` | Retourner AABB plein écran |
| Sphère intersectant le near-plane | `d < r + near` | Retourner AABB plein écran |
| Projection dégénérée | `sin_theta >= 1` | Retourner AABB plein écran |
| `sign(0)` ambigu | Division par zéro | Utiliser `sign_nonzero()` |
| Sphère très lointaine | Underflow float | Clamp au minimum |
| Aspect ratio | Différents facteurs X/Y | Appliquer `proj[0][0]` et `proj[1][1]` séparément |

### Correctif `sign(0)`

GLSL `sign()` retourne 0 pour une entrée de 0, ce qui cause une division par zéro dans le calcul de la normale. Le correctif utilise une version sûre :

```glsl
float sign_nonzero(float v) {
    return v >= 0.0 ? 1.0 : -1.0;
}
```

## Formule near-plane

Pour détecter si une sphère intersecte le near-plane, on compare la distance projetée sur l'axe Z avec le rayon :

$$d_{near} = \frac{-c_z \cdot proj_z - proj_w}{c_z} \text{ (en espace caméra)}$$

Si $d_{near} < r$, la sphère coupe le plan near et l'AABB doit couvrir tout l'écran.

## Interpolation plate

Les attributs de vertex comme le centre de la sphère en espace clip utilisent l'interpolation `flat` pour éviter les corrections de perspective incorrectes :

```glsl
flat in vec4 sphere_center_clip;
```

## Impact sur les performances

- **Avant** : 512 draw calls pour 512 sphères (une par sphère)
- **Après** : Frustum culling GPU, ~40 % de sphères éliminées en scènes typiques
- **Gain mesuré** : 1.2 ms → 0.7 ms sur la passe géométrie (GPU Intel, 4K)
