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
| :--- | :--- | :--- |
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

## 📸 Système de Profils de Caméra

Le système implémente un **Système de Profils de Caméra** où chaque profil regroupe des caractéristiques de rendu spécifiques de marques de caméras légendaires.

### Profils Implémentés

| Profil | Caractéristique | Cible de Simulation |
| :--- | :--- | :--- |
| **Sony (CineStyle)** | Tons moyens pro, hautes lumières douces, 3D LUT | Alpha 7S III / S-Cinetone |
| **Fujifilm (Classic)** | Ombres cyan, contraste élevé, grain organique | X100VI / X-Trans V |
| **Leica (Summarit)** | Roll-off des blancs sévère, grain ultra-fin | M11 Monochrome |
| **Canon (Vivid)** | Primaires saturées, bloom doux, rouges agréables | EOS R5 |

### 🛠️ Débogage Avancé : Lattice 3D LUT (Shift + F10)

Pour visualiser comment une LUT 3D déforme l'espace colorimétrique, un débogueur de lattice dédié peut être activé via `Shift + F10`. Il affiche une grille de points accélérée par GPU représentant le volume RGB, montrant exactement comment le gamut est remappé par le profil actif.

### Feuille de Route Technique (Réalisée)

1. **Support 3D LUT** : Intégration complète du traitement LUT `.cube` pour le mapping de gamut spécifique aux marques.
2. **Bokeh Anamorphique** : Support pour le bokeh de lentille non sphérique (étirement 2.0x) dans le module Depth of Field.
3. **Visualisation Lattice** : Débogage en temps réel de la déformation de l'espace colorimétrique.

## Interaction avec le tonemapping

L'esthétique cinématique est appliquée **après** le tonemapping ACES pour respecter l'ordre de traitement photographique standard :

```bash
HDR linéaire → Tonemapping ACES → Grading cinématique → Grain → sRGB
```

## Voir aussi

- [photographic_standards.md](./photographic_standards.md) — Standards photographiques et échelle EV
- [postprocess_ubo_architecture.md](./postprocess_ubo_architecture.md) — Architecture des paramètres
- [exposure_analysis.md](./exposure_analysis.md) — Système d'exposition
