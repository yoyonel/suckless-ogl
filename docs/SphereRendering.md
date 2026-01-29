# Rendu de Sphères : Transparence et Anti-Aliasing Analytique

Ce document détaille l'implémentation du rendu "High Quality" des instances de sphères (billboards), incluant le tri CPU pour la transparence correcte et une technique d'anti-aliasing analytique pour des bords parfaits.

## 1. Tri des Sphères (Transparency & Sorting)

Pour gérer correctement la transparence Alpha Blending (`GL_SRC_ALPHA`, `GL_ONE_MINUS_SRC_ALPHA`), les objets doivent être dessinés du plus éloigné au plus proche (Back-to-Front) par rapport à la caméra.

### Architecture `SphereSorter`
Le système de tri est encapsulé dans le module `sphere_sorting` (`src/sphere_sorting.c`).

1.  **Données** :
    *   Les instances (`SphereInstance`) sont stockées de manière contiguë.
    *   Une structure intermédiaire `SphereSortEntry` contient `{ index, depth }` pour chaque sphère.
2.  **Algorithme** :
    *   À chaque frame, on calcule la distance au carré (`glm_vec3_distance2`) entre la caméra et chaque sphère.
    *   On utilise `qsort` standard pour trier les clés `SphereSortEntry` par profondeur décroissante (Back-to-Front).
    *   On reconstruit un buffer temporaire d'instances triées.
3.  **Optimisation SIMD** :
    *   Les buffers d'instances sont alloués via `aligned_alloc` avec un alignement de 64 octets (`SIMD_ALIGNMENT`) pour optimiser les accès mémoires et permettre une potentielle vectorisation AVX.

### Pipeline de Rendu
Si la macro `USE_TRANSPARENT_BILLBOARDS` est activée et que le mode est "Transparent" (touche `T`) :
1.  **Rendu Skybox** (en premier, écriture depth).
2.  **Tri CPU** des sphères via `sphere_sorter_sort`.
3.  **Upload** des données triées via `glBufferSubData`.
4.  **Draw** des sphères avec Blending activé et Depth Write **désactivé** (lecture seule).

---

## 2. Anti-Aliasing Analytique ("Perfect AA")

Les sphères ne sont pas de la géométrie 3D réelle mais des **Imposteurs** (Billboards 2D sur un Quad). Le rendu exact de la sphère est calculé mathématiquement pour chaque pixel dans le Fragment Shader (`pbr_ibl_billboard.frag`).

### Problème de l'Aliasing
Si on "coupe" brutalement le pixel quand le rayon ne touche pas la sphère (`discard` si `discriminant < 0`), on obtient des bords en escalier (aliasing) très visibles. Le MSAA ne fonctionne pas bien ici car pour le GPU, c'est un Quad plat.

### Solution : Lissage par le Discriminant
L'équation d'intersection Rayon-Sphère donne un **discriminant** ($\Delta$ ou $h$).
*   $h > 0$ : Intersection (dans la sphère).
*   $h < 0$ : Pas d'intersection (hors de la sphère).
*   $h \approx 0$ : Bord exact de la sphère.

Pour lisser le bord, on utilise la dérivée de la fonction de distance pour estimer la couverture du pixel :
```glsl
// Calcul analytique de l'intersection
float h = b*b - c; // Discriminant

// Si h < 0, on est hors de la sphère.
// Mais proche de 0, on veut un dégradé (alpha transition).

// fwidth(h) nous donne la variation de h sur la largeur d'un pixel.
// Cela nous permet de normaliser h pour savoir "à quelle fraction de pixel" on est du bord.
float edgeFactor = smoothstep(0.0, fwidth(h), h);

// On applique ce facteur à l'alpha ou à la couleur finale
finalColor.a *= edgeFactor;
```
Ce `edgeFactor` assombrit (ou rend transparent) les pixels qui sont à cheval sur le bord mathématique de la sphère, produisant un anti-aliasing **analytiquement parfait**, indépendant de la résolution.

---

## 3. Configuration & Macros
Le comportement est contrôlé par la macro `USE_TRANSPARENT_BILLBOARDS` définie dans `include/app_settings.h` (injectée automatiquement dans les shaders).

*   **Mode "Legacy" (Macro non définie)** :
    *   Rendu Opaque (Depth Test/Write ON).
    *   Pas de tri.
    *   Alpha utilisé pour stocker la Luma (optimisation FXAA).
*   **Mode "Transparent" (Macro définie + Touche T)** :
    *   Rendu Transparent (Blend ON, Depth Write OFF).
    *   Tri Back-to-Front à chaque frame.
    *   Alpha utilisé pour l'opacité (True Transparency).
    *   Le FXAA recalcule la Luma depuis RGB.
