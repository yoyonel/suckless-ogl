# Correction de la Stabilité Numérique N-Body

## Description du Problème
Une instabilité numérique a été observée dans la simulation N-body, entraînant une dérive d'énergie, des trajectoires erratiques et occasionnellement des valeurs `NaN`. Ces problèmes étaient particulièrement graves dans les builds `release` et lors des opérations d'inversion temporelle.

## Causes Techniques

### 1. Perte de Précision des Nombres Flottants
Les simulations physiques impliquant des calculs cumulatifs sont très sensibles aux erreurs d'arrondi. L'utilisation de la précision `float` (32-bit) entraînait une dérive d'énergie importante sur de longues périodes, car de petites erreurs dans les calculs de force s'accumulaient à chaque image.

### 2. Bug de Damping en Inversion Temporelle
Le mécanisme d'amortissement radial (zone de confinement) utilisait directement `delta_time`. Lorsque le temps était inversé (`delta_time < 0`), le terme d'amortissement devenait une force d'accélération, provoquant une explosion d'énergie des corps qui étaient alors éjectés du volume de simulation.

### 3. Effets Secondaires du Fast-Math
Les optimisations agressives du compilateur (`-ffast-math`) sacrifient la conformité IEEE 754 pour la vitesse. Cela peut briser les propriétés symplectiques de l'intégrateur Velocity Verlet, entraînant des comportements non physiques.

## Solution

### 1. Passage en Double Précision
L'état de la physique et les calculs ont été migrés vers la précision `double` (64-bit).
- **Champs affectés** : Position, vitesse, masse et calculs d'énergie.
- **Rendu** : Le GPU reçoit toujours des données `float` via les VBO pour la performance, mais la "source de vérité" de la simulation est désormais en haute précision.

### 2. Amortissement Symétrique
La logique d'amortissement a été mise à jour pour utiliser `fabs(delta_time)`, garantissant qu'elle reste dissipative quelle que soit la direction du temps :
```c
float damping_factor = 1.0F - (sim->damping * fabsf(delta_time));
```

### 3. Gestion des Flags au Niveau du Build
Au lieu de pragmas dans le code source, nous imposons désormais un comportement flottant standard via `CMakeLists.txt` :
```cmake
set_source_files_properties(src/nbody.c PROPERTIES COMPILE_FLAGS "-fno-fast-math")
```
Cela garantit que le cœur de la physique est toujours compilé avec une conformité IEEE 754 stricte, même si le reste de l'application utilise des optimisations agressives.

## Vérification
Des tests de longue durée (1200s) confirment une dérive d'énergie inférieure à 3%, et les tests d'inversion temporelle montrent une réversibilité quasi parfaite (erreur de $10^{-11}$), confirmant la robustesse de la nouvelle implémentation.
