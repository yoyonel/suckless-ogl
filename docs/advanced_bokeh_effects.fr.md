# Effets de Bokeh Avancés : Simulation G-Master & Anamorphique

Recherche technique et stratégie d'implémentation pour les effets optiques haut de gamme dans `suckless-ogl`.

## 1. Caractéristiques des Objectifs Sony G-Master

La gamme G-Master (GM) de Sony est conçue pour résoudre des détails élevés tout en conservant une esthétique de bokeh « fondante ».

### Éléments XA (Asphérique Extrême)

- **Précision de Surface** : Précision de surface de 0,01 micron.
- **Suppression des "Onion Rings"** : Les lentilles asphériques conventionnelles présentent souvent des cercles concentriques dus aux imperfections du moule. Les lentilles GM utilisent une précision de surface extrême pour garantir que les zones floues sont parfaitement lisses.
- **Implémentation Moteur** : Historiquement, le flou procédural ou les faibles taux d'échantillonnage peuvent introduire du bruit. La simulation G-Master nécessite des tampons à haute profondeur de bits (RGBA16F) et des noyaux mathématiques lisses (comme le filtre Kawase) pour maintenir cet aspect « propre ».

### Ouverture Circulaire à 11 Lames

- **Forme** : La plupart des objectifs utilisent 7 ou 9 lames. 11 lames garantissent que même lorsque l'ouverture est réduite, le bokeh reste presque parfaitement circulaire.
- **Implémentation Moteur** : Pour la simulation de bokeh polygonal, nous devrions prendre en charge des nombres de lames élevés (N=11) dans la passe de diffusion du bokeh.

---

## 2. Effets d'Objectifs Anamorphiques

Les lentilles anamorphiques utilisent un élément cylindrique pour « compresser » une image large sur un capteur standard, qui est ensuite « décompressée » en post-production.

### Bokeh Ovale

- **Cause Optique** : Le facteur de compression cylindrique (généralement 1,33x, 1,5x ou 2,0x) affecte les zones floues avant la décompression.
- **Effet** : Les hautes lumières floues apparaissent comme des ovales verticaux au lieu de cercles.
- **Implémentation Moteur** : Appliquer un `anamorphic_ratio` (ex: 2,0) au noyau de flou dans la passe de DOF.

### Arrière-plans en "Cascade" (Waterfall)

- **Effet** : Étirement vertical des éléments d'arrière-plan, créant un sentiment de hauteur et de portée « épique ».
- **Implémentation Moteur** : Rayons de flou asymétriques (Flou Vertical > Flou Horizontal).

### Stries Bleues et Flares

- **Cause Optique** : Réflexions entre les éléments cylindriques.
- **Implémentation Moteur** : Une passe de Bloom spécialisée ou une superposition de Flare qui répond aux stries horizontales de haute luminance.

---

## 3. État de l'Implémentation (POC)

### Phase 1 : Kawase Symétrique vs Asymétrique [IMPLÉMENTÉ]

Modification des shaders `Bloom Downsample` (13 échantillons) et `Bloom Upsample` (filtre tente à 9 échantillons) pour prendre en charge un facteur `texelScale`.

- `dof_anamorphic_ratio` (par défaut 1,0) est passé comme `vec2(1.0, ratio)` à ces shaders pendant les passes de pré-flou de la DOF.
- Cela crée l'étirement ovale vertical caractéristique des éléments d'arrière-plan avant qu'ils ne soient recomposés dans la scène principale.

### Phase 2 : UBO et Paramètres [IMPLÉMENTÉ]

Ajout de `dof_anamorphic_ratio` au `PostProcessUBO` et aux `DoFParams`.

- Synchronisé entre `include/postprocess.h` and `shaders/postprocess/ubo.glsl`.
- Intégré dans tous les préréglages dans `include/postprocess_presets.h`.
- **Préréglage Sony A7S III** : Utilise désormais un ratio anamorphique de `2,0x` par défaut pour un caractère cinématique distinct.

### Phase 3 : Diffusion de Bokeh (Optionnel) [PLANIFIÉ]

Si une qualité supérieure est requise, implémenter une passe de diffusion pour les pixels de haute luminance afin de dessiner des formes de bokeh polygonales réelles (ex: polygones à 11 lames).
