# Rendu cinématique

Ce document décrit les paramètres et la philosophie du rendu cinématique « Nordic Noir » du projet.

## Philosophie visuelle

Le style cinématique s'inspire des photographies de paysages nordiques et de la direction artistique de Fujifilm pour les films couleur analogiques. L'objectif est une image avec :

- Des hautes lumières douces, sans surbrillances agressives
- Des tons moyens légèrement désaturés et froids
- Des ombres profondes avec une touche de cyan/bleu
- Un grain de film subtil qui ajoute de la texture et du réalisme

## Brume atmosphérique

La brume est appliquée dans le shader de post-traitement selon une formule de densité exponentielle :

$$fog = e^{-density \times depth^2}$$

Les paramètres sont :

| Paramètre | Plage | Valeur par défaut |
|-----------|-------|-------------------|
| `u_fog_density` | 0.0 – 0.5 | 0.05 |
| `u_fog_color` | RGB | `(0.7, 0.8, 0.9)` — bleu ciel froid |
| `u_fog_start` | 0.0 – 100.0 | 10.0 |

## Grain de film

Le grain est généré procéduralement dans le shader :

```glsl
// Bruit sous-pixel basé sur le hash des coordonnées et du numéro d'image
float grain = hash(uv * resolution + frame_count) * grain_strength;
color.rgb += grain - grain_strength * 0.5; // centré sur 0
```

Cela crée un grain variable d'une image à l'autre (pas de motif statique) qui ressemble au grain argentique.

## Profiles de caméra

Des profils de caméra prédéfinis seront disponibles pour simuler différentes esthétiques :

| Profil | Inspiration | Caractéristiques |
|--------|------------|-----------------|
| **Nordic** | Leica M10 Monochrome | Contraste élevé, désaturation douce |
| **Fujifilm Velvia** | Fujifilm Velvia 50 | Couleurs saturées, verts intenses |
| **Cinema Log** | ARRI LogC | Plage dynamique maximale, flat |
| **Neutral** | Rendu physiquement exact | Aucune correction de couleur |

*(La sélection des profils est en cours de développement)*

## Interaction avec le tonemapping

L'esthétique cinématique est appliquée **après** le tonemapping ACES pour respecter l'ordre de traitement photographique standard :

```
HDR linéaire → Tonemapping ACES → Grading cinématique → Grain → sRGB
```

## Voir aussi

- [photographic_standards.md](./photographic_standards.md) — Standards photographiques et échelle EV
- [postprocess_ubo_architecture.md](./postprocess_ubo_architecture.md) — Architecture des paramètres
- [exposure_analysis.md](./exposure_analysis.md) — Système d'exposition
