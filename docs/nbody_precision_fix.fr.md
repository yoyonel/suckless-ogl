# Correction de la Stabilité Numérique N-Body

## Description du Problème
Une instabilité numérique a été observée dans la simulation N-body, entraînant occasionnellement des valeurs `NaN` (Not a Number) dans les vitesses des corps ou des trajectoires erratiques. Ces problèmes provenaient principalement des effets secondaires des optimisations de calculs flottants et de vecteurs de vitesse initiale non normalisés.

## Causes Techniques

### 1. Optimisations Fast-Math
Les compilateurs utilisent souvent `-ffast-math` ou des optimisations agressives similaires pour améliorer les performances. Cependant, ces optimisations :
- Sacrifient la conformité stricte à la norme IEEE 754.
- Permettent la réassociation d'opérations mathématiques (ex: `(a + b) + c` devient `a + (b + c)`), ce qui peut entraîner une perte de précision significative dans les simulations physiques itératives.
- Supposent qu'aucune valeur `NaN` ou `Inf` n'existe, ce qui conduit à un comportement indéfini lorsqu'elles surviennent (ex: lors de rencontres rapprochées entre corps où les forces sont extrêmement élevées).

### 2. Directions de Vitesse non Normalisées
Lors de l'initialisation dans `nbody_init_preset`, la vitesse orbitale était calculée en mettant à l'échelle un vecteur de direction (`orb->vel_dir`). Si ce vecteur n'était pas de longueur unitaire, la vitesse résultante était incorrectement mise à l'échelle, ce qui pouvait entraîner des vitesses extrêmes et une divergence immédiate de la simulation.

## Solution

### Désactivation de Fast-Math pour la Physique
Le cœur de la simulation dans `src/nbody.c` désactive désormais explicitement les optimisations fast-math via un pragma du compilateur :
```c
#pragma GCC optimize ("no-fast-math")
```
Cela garantit que l'intégrateur Velocity Verlet conserve une précision maximale et gère correctement les cas limites numériques (comme les petites distances ou les NaNs) selon les règles standard des virgules flottantes.

### Normalisation de la Direction de Vitesse
La logique d'initialisation garantit désormais que le vecteur de direction de la vitesse est normalisé avant d'être mis à l'échelle par la vitesse orbitale :
```c
vec3 normalized_vel_dir;
glm_vec3_copy((float*)orb->vel_dir, normalized_vel_dir);
glm_vec3_normalize(normalized_vel_dir);

vec3 vel = {normalized_vel_dir[0] * spd, normalized_vel_dir[1] * spd,
            normalized_vel_dir[2] * spd};
```

### Détection de NaN
Un contrôle de diagnostic a été ajouté à l'étape d'intégration pour afficher un avertissement si la vitesse d'un corps devient `NaN`, facilitant ainsi le débogage de futurs problèmes de stabilité.
