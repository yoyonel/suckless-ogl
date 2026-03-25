# Analyse du système d'exposition

Ce document décrit en détail le pipeline d'exposition, des modes manuels et automatiques aux shaders de calcul.

## Modes d'exposition

### Exposition manuelle

L'exposition manuelle est contrôlée via la valeur EV (Exposure Value) :

$$L_{out} = L_{in} \times 2^{EV}$$

Des valeurs EV positives surexposent (éclaircissent), négatives sous-exposent (assombrissent).

Contrôles :
- `+` / `-` : Ajuster l'EV par incréments de 0.1
- `R` : Réinitialiser à EV 0.0

### Exposition automatique

L'exposition automatique mesure la luminance moyenne de la scène et ajuste l'EV en conséquence pour cibler une luminance de sortie de 18 % (gris moyen).

$$EV_{target} = \log_2\left(\frac{L_{avg}}{0.18}\right)$$

L'adaptation est progressive (effet « œil humain ») :

```glsl
// Shader de compute lum_adapt.comp
float current = imageLoad(u_lum_current, ivec2(0, 0)).r;
float target = u_lum_target;
float rate = u_adaptation_speed; // 0.05 par défaut
float adapted = current + (target - current) * (1.0 - exp(-rate * u_delta_time));
imageStore(u_lum_current, ivec2(0, 0), vec4(adapted));
```

## Pipeline GPU

Le pipeline d'exposition s'intercale **avant le tonemapping** :

```
Image HDR linéaire
    ↓
Downsampling de luminance (lum_downsample.frag)
    ↓  [réduction log-average]
Adaptation temporelle (lum_adapt.comp)
    ↓  [EV progressif]
Application de l'exposition (multiply dans tonemapping)
    ↓
Tonemapping ACES
    ↓
Image LDR sRGB finale
```

### Downsampling de luminance

Le shader `lum_downsample.frag` réduit l'image en calculant la moyenne logarithmique de la luminance sur une grille de 4×4 pixels à chaque passe :

```glsl
// Calcul de la luminance perceptuelle
float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
// Compression logarithmique pour les scènes HDR
float log_luma = log2(luma + 0.0001);
```

La moyenne logarithmique est préférable à la moyenne linéaire car elle correspond mieux à la perception humaine de la luminosité.

## Vérification du niveau de noir

L'exposition automatique peut causer des artefacts dans les scènes très sombres si la luminance calculée approche zéro. Un plancher de luminance est appliqué :

```c
float lum_floor = 0.0001f; // Évite log(0)
float lum_clamped = fmaxf(lum_avg, lum_floor);
```

## Readback asynchrone

La luminance adaptée est lue depuis le GPU vers le CPU une fois par image pour affichage dans le HUD (F1) :

- Utilise le protocole PBO double tampon (pas de blocage)
- La valeur affichée a un décalage d'une image (N-1), imperceptible

## Tableau des paramètres

| Paramètre | Plage | Défaut | Description |
|-----------|-------|--------|-------------|
| `u_ev_manual` | −10.0 à +10.0 | 0.0 | EV d'exposition manuelle |
| `u_auto_exposure` | 0 / 1 | 0 | Activer l'exposition automatique |
| `u_adaptation_speed` | 0.01 à 2.0 | 0.05 | Vitesse d'adaptation (secondes) |
| `u_lum_min` | −10.0 à 0.0 | −4.0 | Luminance minimale (log2) |
| `u_lum_max` | 0.0 à 10.0 | 4.0 | Luminance maximale (log2) |

## Voir aussi

- [auto_exposure_debug_histogram.md](./auto_exposure_debug_histogram.md) — Histogramme de débogage
- [photographic_standards.md](./photographic_standards.md) — Standards photographiques et échelle EV
- [postprocess_ubo_architecture.md](./postprocess_ubo_architecture.md) — Architecture des paramètres
