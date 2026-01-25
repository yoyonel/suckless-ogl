# Analyse Globale de la Gestion de l'Exposition
**Date : 24 Janvier 2026**

Cette analyse détaille comment l'exposition est définie, gérée, et appliquée dans l'application, en couvrant à la fois le CPU (gestion) et le GPU (implémentation et exposition automatique).

## 1. Définition et Structure des Données

L'exposition est gérée via deux modes distincts qui ne se mélangent pas : **Manuel** et **Automatique**.

### Côté CPU (`include/postprocess.h`, `src/postprocess.c`)
Les paramètres sont stockés dans la structure `PostProcess` :

*   **Exposition Manuelle** :
    *   Struct : `ExposureParams`
    *   Variable : `exposure` (float).
    *   Défaut : `1.0` (Défini par `DEFAULT_EXPOSURE`).
    *   Contrôle : `postprocess_set_exposure()`.

*   **Exposition Automatique (Eye Adaptation)** :
    *   Struct : `AutoExposureParams`
    *   Variables :
        *   `min_luminance` / `max_luminance` : Plage de clamping de la luminance de la scène.
        *   `speed_up` / `speed_down` : Vitesse d'adaptation (l'œil s'adapte plus vite à la lumière qu'à l'obscurité).
        *   `key_value` : La valeur de gris moyen cible (Ancre de l'exposition).
    *   Contrôle : `postprocess_set_auto_exposure()`.

### Côté GPU (Uniforms & Textures)
*   **Manuel** : Transmis via l'uniform `exposure.exposure`.
*   **Automatique** :
    *   `autoExposureTexture` : Texture 1x1 `GL_RGBA32F` (Image Load/Store) qui stocke l'état persistant de l'exposition.
    *   `lumTexture` : Texture 64x64 `GL_R16F` utilisée comme étape intermédiaire pour le calcul de luminance moyenne.

---

## 2. Runpaths et Modes d'Utilisation

Le choix du mode se fait dans la boucle de rendu `postprocess_end()`.

### Mode A : Exposition Automatique (`POSTFX_AUTO_EXPOSURE`)
Si ce flag est activé, une passe de pré-calcul est exécutée **avant** le rendu final.

1.  **Downsample (Réduction de Luminance)**
    *   **Fichier** : `shaders/lum_downsample.frag`
    *   **Action** : Prend la scène HDR (`scene_color_tex`) et la réduit en une texture 64x64.
    *   **Logique** :
        *   Échantillonne des blocs 4x4.
        *   Calcule la luminance : `dot(color, vec3(0.2126, 0.7152, 0.0722))`.
        *   Convertit en logarithme : `log2(lum)`.
        *   Moyenne les valeurs valides (ignore les noirs purs/infinis).
    *   **But** : Préparer une donnée compacte pour le Compute Shader.

2.  **Adaptation (Compute Shader)**
    *   **Fichier** : `shaders/lum_adapt.comp`
    *   **Action** : Calcule l'exposition finale avec inertie temporelle.
    *   **Logique** (Thread unique 1,1,1) :
        *   Lit la texture 64x64 et calcule la moyenne globale du Log Luminance.
        *   Convertit en Luminance Scène linéaire : `exp2(avgLogLum)`.
        *   Clamp la luminance entre `minLuminance` et `maxLuminance`.
        *   Calcule la cible : `targetExposure = keyValue / sceneLum`.
        *   **Interpolation** : Lit l'exposition *précédente* dans l'image 1x1 et interpole vers la cible selon `deltaTime` et `speedUp`/`speedDown`.
        *   Ecrit le nouveau résultat dans l'image 1x1.

3.  **Application Finale**
    *   **Fichier** : `shaders/postprocess/exposure.glsl` (inclus dans `postprocess.frag`).
    *   La fonction `getCombinedExposure()` détecte que l'auto-exposure est active.
    *   Elle échantillonne la texture 1x1 : `texture(autoExposureTexture, vec2(0.5)).r`.

### Mode B : Exposition Manuelle (`POSTFX_EXPOSURE`)
Utilisé si l'auto-exposure est désactivée.

1.  Les passes de Downsample et Compute Shader sont **ignorées**.
2.  **Application Finale** :
    *   `getCombinedExposure()` utilise directement l'uniform `exposure.exposure`.

### Mode C : Pas d'exposition
Si aucun effet n'est actif, la valeur `1.0` est utilisée.

---

## 3. Intégration dans le Pipeline (`postprocess.frag`)

L'exposition est appliquée à une étape précise du fragment shader final :

1.  Input (Couleur Scène).
2.  Chrom. Aberration / Motion Blur.
3.  Depth of Field.
4.  Bloom (Ajouté à la couleur).
5.  **>>> EXPOSITION <<<** : `color *= finalExposure`.
6.  Color Grading & White Balance.
7.  Tone Mapping (Passage de HDR à LDR).
8.  Gamma Correction.

**Observation Importante** : L'exposition est appliquée comme un simple multiplicateur scalaire sur le signal HDR linéaire. C'est physiquement cohérent (simulation de l'ouverture/ISO de la caméra/pupille) et cela se produit correctement avant le Tone Mapping.

## 4. Pré-calculs et Optimisations

*   **Downsample 64x64** : Plutôt que de faire une réduction parallèle complexe de toute l'image 1080p/4k, l'image est d'abord réduite drastiquement via un fragment shader très rapide (`lum_downsample.frag`) qui fait une moyenne approximative (Box Filter 4x4 avec stride). Cela réduit massivement la charge pour le Compute Shader.
*   **Compute Shader Unique** : L'adaptation ne lance qu'un seul WorkGroup (1,1,1) car il ne traite que 4096 pixels (64x64). C'est extrêmement léger.
*   **Texture Persistante** : L'utilisation d'une texture 1x1 comme stockage permet de conserver l'état de l'exposition d'une frame à l'autre sans avoir besoin de faire des aller-retours CPU-GPU (pas de `glGetTexImage` nécessaire pour la logique, tout reste sur le GPU).

## Résumé Synthétique

| Aspect | Détail |
| :--- | :--- |
| **Stockage État** | Texture 1x1 Float (GPU) |
| **Logique Adaptation** | Compute Shader (`lum_adapt.comp`) |
| **Métrique** | Moyenne Logarithmique (Log Average Luminance) |
| **Entrée Analyse** | Texture downsamplée 64x64 |
| **Application** | Multiplication scalaire dans `postprocess.frag` |
| **Conflit** | L'Auto-Exposure est prioritaire sur l'Exposition Manuelle |
