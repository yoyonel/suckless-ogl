# Analyse de l'implémentation du Motion Blur

Cette documentation détaille l'état actuel de l'implémentation du Motion Blur dans **Suckless OGL**, ses performances, sa qualité, et les compare avec les approches des moteurs AAA, en s'appuyant particulièrement sur le cas du *RE Engine* (utilisé pour *Street Fighter 6*). Elle offre également des pistes d'amélioration concrètes.

---

## 1. Architecture Actuelle

L'architecture actuelle repose sur des principes modernes de rendu basés sur l'approche **Tile-Based / Neighbor Max**, initialement introduite par Jean-Yves Bouguet et les chercheurs en rendu temps réel. L'idée principale est d'éviter les "fuites" de flou lorsqu'un objet rapide passe devant un arrière-plan fixe, un artefact très commun dans les premières implémentations de ce post-process.

### Pipeline de rendu

```mermaid
graph TD
    V[Velocity Buffer] --> T[Tile Max Velocity Compute]
    T -->|Réduction à 16x16 via Shared Memory| TTex(Texture RG16F - Tile Max)
    TTex --> N[Neighbor Max Velocity Compute]
    N -->|textureGather sur un Voisinage 3x3| NTex(Texture RG16F - Neighbor Max)
    C[Color Buffer Raw] --> M(Passe Motion Blur Finale)
    D[Depth Buffer] --> M
    V --> M
    NTex --> M
    M -->|Échantillonnage de 8 frames \n+ Interleaved Gradient Noise \n+ Depth Weighting| O[Color Buffer Flouté]
```

### 💡 Points forts de la solution

1. **Utilisation intensive des Compute Shaders :** L'approche en deux passes (`tile_max` et `neighbor_max`) est performante. La réduction dans la mémoire partagée (Shared Memory) pour l'étape Tile Max minimise considérablement la bande passante. L'utilisation intelligente du `textureGather` permet une lecture optimale en quads.
2. **Neighbor Max Velocity :** Empêche efficacement le flou de se propager de manière incohérente sur les objets et supprime les halos disgracieux.
3. **Pondération avec la Profondeur (Depth Weighting) :** Protège le premier plan en désactivant le mélange de couleurs lorsque l'échantillon prélevé est trop éloigné derrière le pixel central (`depthDiff > 1.0`).
4. **Bruit de Gradient (Interleaved Gradient Noise) :** Empêche le phénomène de *banding* visuel en transformant les stries liées à un faible nombre d'échantillons en bruit esthétique de style pellicule.

### ✅ Vélocité Per-Object (Implémentée)

Depuis la branche `feat/per-object-motion-blur`, le velocity buffer prend désormais en compte **à la fois le mouvement caméra et le mouvement per-objet**. C'était la limitation la plus critique de l'approche camera-only initiale.

#### Chemin de rendu instancié (`pbr_ibl_instanced.vert`)

Un nouvel attribut vertex `i_prev_center` (location 8) stocke la position du centre de la sphère à la frame précédente. Le vertex shader reconstruit la position monde précédente :

```glsl
vec3 prevWorldPos = i_prev_center + (WorldPos - vec3(i_model[3]));
PreviousClipPos = previousViewProj * vec4(prevWorldPos, 1.0);
```

Cela capture à la fois le mouvement caméra (via `previousViewProj`) et le mouvement objet (via `i_prev_center != centre actuel`).

#### Chemin Billboard/SSBO (`pbr_ibl_billboard.vert` / `.frag`)

La struct SSBO stocke `prev_center_x/y/z` en 3 floats séparés (pas `vec3`, pour respecter l'alignement std430 à l'offset 92). Le fragment shader reconstruit :

```glsl
vec3 prevHitPos = PrevSphereCenter + (sphereHitPos - SphereCenter);
vec4 prevClip = previousViewProj * vec4(prevHitPos, 1.0);
```

#### Flux de données

- `NBodyParticle.prev_position` capture la position avant chaque pas physique
- `nbody_write_instances()` copie `prev_position` → `SphereInstance.prev_center`
- Les sphères statiques de la grille ont `prev_center == centre actuel` → vélocité objet nulle
- `SphereInstance` reste à 128 octets (aligné SIMD, validé par `_Static_assert`)

### 🐛 Correction d'artefact des VFX transparents (glColorMaski)

Les passes de rendu transparentes (trails, shockwaves) écrivaient des valeurs indéfinies dans le velocity buffer (color attachment 1) car leurs shaders ne possèdent pas de sortie vélocité. Sur certains GPU (notamment NVidia 950m sous Bazzite), cela causait des artefacts de motion blur visibles — des traînées et du ghosting dans les zones couvertes par les effets transparents.

**Cause racine :** Quand un fragment shader n'écrit pas explicitement dans `gl_FragData[1]` (ou la sortie `layout(location=1)` équivalente), le GPU peut écrire des valeurs parasites ou des registres obsolètes dans cet attachment si l'écriture est activée.

**Correction :** Utiliser `glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE)` avant les draw calls des VFX transparents pour désactiver l'écriture dans le velocity buffer, et restaurer avec `glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)` après :

```c
/* Désactiver l'écriture dans le velocity buffer (Attachment 1) */
glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
/* ... rendu de la géométrie transparente ... */
glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
```

Note : Le renderer de billboards gérait déjà ce cas via `glDisablei(GL_BLEND, 1)` dans `scene_render.c`, mais les trails et shockwaves utilisaient un chemin de rendu différent sans cette protection.

---

## 2. Pistes d'Améliorations (Vitesse et Qualité)

Afin d'atteindre l'état de l'art, voici les optimisations et changements possibles.

### A. Vitesse et Optimisation

* **Échantillonnage Adaptatif (Dynamic Sample Count) :**
  Actuellement, le shader effectue **toujours** `mb_samples` itérations (8 par défaut), même si la vitesse du pixel est proche de zéro (`speed < 0.0001` est ignoré, mais les faibles valeurs passent). Un nombre dynamique d'itérations basé sur la vitesse proportionnelle permettrait d'économiser un nombre monumental de cycles GPU.
  ```glsl
  int actual_samples = max(2, int(speed * TARGET_SAMPLES_PER_UNIT));
  ```
* **Half-Resolution Reconstruction (TODO) :**
  Exécuter 8+ lectures aléatoires non-cohérentes avec le cache sur un buffer 4K ou 1440p est extrêmement gourmand. L'accumulation du Motion Blur dans une **toute petite texture** (à moitié de la résolution native) suivie d'un *upsample* bilatéral guidé par la profondeur est à implémenter pour les performances futures.

    !!! warning "Plan d'implémentation (Refactoring de `postprocess.c`)"
        L'implémentation du Motion Blur est actuellement incluse dans le composite unifié de la passe finale (`postprocess.frag`). Pour appliquer ce *Half-Res*, il faudra :

        1. Restructurer le rendu et casser le flux actuel en désolidarisant le Motion Blur de cette maxi-passe.
        2. Créer un nouveau `Framebuffer` dédié au sous-échantillonnage de dimensions `width/2 x height/2`.
        3. Créer une passe spécifique (via compute ou quad écran) ne calculant *uniquement* que l'effet de flou dans cette texture allégée en couleurs et vélocités.
        4. Écrire et exécuter l'étape d'*upsample* bilatéral durant la passe `postprocess.frag` ou dans une passe dédiée, afin de recomposer judicieusement l'image finale sur les bords des objets définis par le `depth buffer`.

* **Per-Object et Skinned Velocity :**
  ~~Pour que le motion blur s'applique aux objets en mouvement ou aux personnages, il faut stocker et transmettre la matrice Modèle de la frame précédente pour *chaque* objet.~~ ✅ **Implémenté** — la vélocité per-objet est désormais active via `prev_center` dans `SphereInstance`. La prochaine étape est la vélocité skinned per-vertex pour les maillages animés.
* **Soft Depth-Testing :**
  Remplacer le test booléen et brutal de profondeur par une pondération lissée à l'aide d'un `smoothstep()`. Des limites dures créent un bruit granuleux ou des tremblements de pixels (flickering) sur les arêtes en mouvement.

### C. Le Cas Exceptionnel : Flou Analytique Procédural

Puisque **Suckless OGL** rend nativement des objets purs via **Raytracing analytique** dans le Fragment Shader (par le biais des billboards et *impostors* de sphères dans `pbr_ibl_billboard.frag`), une solution mathématiquement parfaite et sans post-process (2D) est techniquement possible.

Lorsqu'une sphère de centre $C$ se déplace d'une position $C_0$ vers $C_1$ (via un vecteur vitesse $\vec{V}$) au cours d'une frame, le volume balayé par cette sphère (*Swept Volume*) forme géométriquement une **Capsule** (un cylindre terminé par deux hémisphères).

**Comment l'implémenter mathématiquement au lieu du Post-Process ?**

1. **Intersection Rayon-Capsule :** Au lieu de croiser un rayon avec une sphère, le shader des *impostors* calcule l'intersection analytique d'un rayon avec une capsule s'étendant du centre à $t=0$ au centre à $t=1$.
2. **Intégration Temporelle :** Une fois l'intersection résolue sur la droite de mouvement de la capsule, l'opacité (Alpha) et la normale de surface en ce point peuvent être calculées en fonction du temps couvert par le croisement.
3. **Mouvement de la Caméra + Mouvement de l'Objet :** La matrice de transformation de la vue au cours du temps peut simplement modifier l'origine du rayon $O(t) = O_0 + V_{cam} \cdot t$ pour simuler le flou de caméra.
4. **Jittering Temporel / Stochasticité :** Au lieu d'accumuler dans un multi-buffer, le rayon généré depuis le point central de l'écran peut se voir attribuer une variable temps $t$ aléatoire par frame (via du Bruit Bleu par exemple). Couplé à un TAA (Temporal Anti-Aliasing), le flou de mouvement devient instantanément "gratuit" et mathématiquement parfait (vrai flou 3D) sans les erreurs d'occlusion du post-processing 2D.

C'est une optimisation très avancée mais extrêmement élégante, souvent réservée aux moteurs de rendu hors-ligne (Pixar/RenderMan) ou aux *demoscenes* purement procédurales.

---

## 3. Comparaison avec les Moteurs AAA

### Unreal Engine 5 & Unity (HDRP)

Ces moteurs utilisent une architecture extrêmement similaire au niveau du *Tile Max* / *Neighbor Max*. Cependant, ils intègrent l'effet au sein de leur écosystème global temporel :
* **Découplage :** Ces moteurs comportent un contrôle séparant explicitement la force du flou de mouvement de la Caméra et celui des Objets, car le flou de mouvement de caméra est une cause très connue de *Motion Sickness* chez les joueurs.
* **TSR / TAA :** Chez l'UE5, le flou de mouvement n'est pas uniquement un effet de post-process jetable, il aide visuellement la résolution temporelle (Temporal Super Resolution) en combinant les vecteurs de mouvement à l'Anti-Aliasing.

---

## 4. L'Exemple de Street Fighter 6 (RE Engine)

Le **RE Engine** (Capcom) livre un *Masterclass* sur l'utilisation poussée de l'effet dans le jeu de combat **Street Fighter 6** :

1. **Per-Vertex Velocity Hyper-Précise :**
   Dans SF6, la vélocité n'est pas seulement passée par *mesh*, mais elle est calculée os par os via le shader de *Skinning*. Lorsqu'un personnage donne un coup de pied, sa chaussure développe un vecteur de vélocité gigantesque comparé à sa cuisse.

2. **Sur-Échantillonnage Localisé (Targeted High-Samples) :**
   Pour économiser de la puissance sans sacrifier la qualité, le RE Engine se permet de lancer entre 16 et 32 échantillons (Samples) par pixel, **mais uniquement** sur les silhouettes enregistrant des vecteurs de haute amplitude. L'environnement statique derrière reste parfaitement net et très peu coûteux à évaluer.

3. **Motion Blur Courbe (Curved Motion Blur) :**
   Contrairement à du flou Linéaire standard (« je prends un vecteur et je trace une ligne droite »), le moteur de Capcom stocke l'information de l'accélération en plus de la vitesse. L'échantillonnage de Flou est ainsi "courbé" dans l'espace afin de simuler la trajectoire radiale des membres et poings.

```mermaid
graph LR
    A[Flou Linéaire \nStandard Suckless OGL] -->|Crée des lignes droites et des artefacts| B(Trajectoire d'un coup de poing)
    C[Flou Courbe \nRE Engine / SF6] -->|Échantillonnage le long d'un arc de cercle| D(Flou stylisé style Anime/Manga)
```

### Le Verdict
Suckless OGL implémente désormais le **motion blur per-objet** via le stockage de `prev_center` dans les données d'instance, couvrant à la fois la vélocité caméra et objet dans le velocity buffer. Les pistes d'amélioration restantes sont : la **vélocité skinned per-vertex** pour les maillages animés, la **reconstruction demi-résolution** pour les performances, et l'**échantillonnage adaptatif** basé sur l'amplitude de vélocité par pixel.
