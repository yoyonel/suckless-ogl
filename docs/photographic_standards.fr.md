# Standards photographiques

Ce document établit les bases théoriques des standards photographiques utilisés dans le moteur PBR.

## Le gris moyen à 18 %

La référence de luminance centrale en photographie est le **gris moyen à 18 %**, qui correspond à la réflectance perceptuelle du gris neutre. Toutes les mesures d'exposition sont calibrées pour que 18 % de réflectance linéaire soit mappé au point médian de la courbe tonale.

$$L_{mid} = 0.18$$

## Échelle EV (Exposure Value)

L'EV quantifie la quantité de lumière reçue par le capteur :

$$EV = \log_2\left(\frac{N^2}{t}\right)$$

où $N$ est le nombre f (ouverture du diaphragme) et $t$ est le temps d'exposition en secondes.

### Correspondances EV typiques

| EV | Scène type | Luminance (cd/m²) |
|----|-----------|-------------------|
| −4 | Nuit sans lune | 0.001 |
| 0 | Nuit étoilée | 0.01 |
| 5 | Crépuscule | 1 |
| 10 | Intérieur éclairé | 50 |
| 14 | Extérieur couvert | 1 000 |
| 17 | Soleil direct | 10 000 |

## Albédos des matériaux

Les albédos (réflectances diffuses) physiquement corrects pour les matériaux courants :

| Matériau | Albédo linéaire | Albédo sRGB |
|----------|----------------|-------------|
| Charbon | 0.04 | 0.22 |
| Gazon humide | 0.08 | 0.31 |
| Béton | 0.14 | 0.41 |
| Sable | 0.22 | 0.50 |
| Neige fraîche | 0.80 | 0.91 |
| Métaux | Métal = 0.7–1.0 × F0 | — |

## Courbes de tonemapping

### Reinhard simple

$$L_{out} = \frac{L_{in}}{1 + L_{in}}$$

Avantages : Simple, stable. Inconvénients : Désature les hautes lumières.

### Hable (Uncharted 2)

Conçue par John Hable pour les jeux vidéo, offre des ombres détaillées et des hautes lumières douces inspirées de l'aspect filmique.

### ACES (Academy Color Encoding System)

Standard industriel used in cinema. L'approximation utilisée dans le projet :

```glsl
vec3 aces_approx(vec3 v)
{
    v *= 0.6;
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((v * (a * v + b)) / (v * (c * v + d) + e), 0.0, 1.0);
}
```

## Formules d'exposition automatique

### EV cible depuis la luminance

$$EV_{target} = \log_2\left(\frac{\bar{L}}{0.18}\right)$$

### Adaptation temporelle (œil humain)

$$EV_{current}(t) = EV_{current}(t-1) + (EV_{target} - EV_{current}(t-1)) \times (1 - e^{-k \Delta t})$$

où $k$ est la vitesse d'adaptation (typiquement 0.05 pour reproduire la réponse de l'œil humain à la lumière).

## Standards de couleur

| Espace | Gamma | Gamut | Usage |
|--------|-------|-------|-------|
| **Linear** | 1.0 | sRGB D65 | Calculs GPU internes |
| **sRGB** | ~2.2 | sRGB D65 | Affichage final |
| **ACEScg** | 1.0 | ACES AP1 | Étalonnage cinématique |
| **ACEScc** | Log | ACES AP1 | Grading colorimétrique |

Tous les calculs PBR sont effectués dans l'espace **linéaire**. La conversion sRGB s'applique uniquement en sortie finale (ou via `GL_FRAMEBUFFER_SRGB`).

## Voir aussi

- [exposure_analysis.md](./exposure_analysis.md) — Implémentation de l'exposition
- [cinematic_rendering.md](./cinematic_rendering.md) — Esthétique cinématique
