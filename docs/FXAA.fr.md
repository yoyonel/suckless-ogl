# FXAA 3.11 — Anti-aliasing temporel rapide

Ce document décrit l'intégration et le comportement de l'algorithme **FXAA 3.11** dans ce moteur de rendu.

## Vue d'ensemble

FXAA (Fast Approximate Anti-Aliasing) est un algorithme de post-traitement qui atténue le crénelage en détectant et en lissant les bords directement dans l'espace écran, sans information géométrique supplémentaire.

La version 3.11 utilisée ici est intégrée sous forme d'un **uber-shader** unique : `shaders/postprocess/fxaa.glsl`.

## Modes de luminance

FXAA opère sur la luminance perceptuelle pour détecter les bords. Trois modes d'entrée sont supportés :

| Mode | Source de luminance | Utilisation recommandée |
|------|-------------------|------------------------|
| `0` | Canal alpha de la texture | Optimal — pré-calculé en amont dans le shader PBR |
| `1` | Luminance calculée depuis RGB | Compatible mais plus coûteux |
| `2` | Composante verte uniquement | Approximation rapide acceptable |

Le projet utilise le **mode 0** : la luminance est stockée dans le canal alpha de la texture de post-traitement lors de la passe PBR, ce qui évite tout recalcul.

## Paramètres de configuration

| Paramètre | Valeur par défaut | Description |
|-----------|------------------|-------------|
| `FXAA_QUALITY__PRESET` | `39` | Qualité maximale — 12 passes d'affinement |
| `FXAA_EDGE_THRESHOLD` | `0.063` | Seuil de contraste minimal pour détecter un bord |
| `FXAA_EDGE_THRESHOLD_MIN` | `0.0312` | Ignore les bords dans les zones très sombres |
| `FXAA_SUBPIX` | `0.75` | Force du lissage sous-pixel |

## Étapes de l'algorithme

```
1. Lecture de la luminance        → Alpha de la texture couleur
2. Détection des bords locaux     → Contraste entre voisins
3. Orientation du bord            → Horizontal ou vertical ?
4. Recherche des extrémités       → Parcours le long du bord dans les deux sens
5. Calcul du gradient de bord     → Évaluation de la longueur relative
6. Sous-pixel blending            → Décalage UV proportionnel à l'estimation
7. Retour couleur lissée          → texture() avec UV corrigé
```

## Optimisations spécifiques au projet

### Canal alpha comme source de luminance

Pré-calculer la luminance dans le shader PBR élimine `dot(color.rgb, vec3(0.299, 0.587, 0.114))` lors de la passe FXAA. Cette décision :
- Réduit les lectures de texture redondantes
- Améliore la cohérence entre le calcul PBR et le FXAA
- Corrige les différences de comportement entre Intel et NVIDIA (voir [synchronisation GPU](./gpu-rendering-synchronization.md))

### Précision du luma dans la boucle de recherche

La boucle d'affinement des extrémités lit le luma directement depuis le canal alpha au lieu de le recalculer par `sqrt()`. Cela améliore la précision et la cohérence entre pilotes.

```glsl
// Avant (source de divergence entre fournisseurs)
lumaEnd1 = FxaaLuma(texture(screenTexture, uv1).rgb);

// Après (stable sur tous les GPU)
lumaEnd1 = texture(screenTexture, uv1).a;
```

## Débogage

La passe FXAA peut être désactivée via le contrôle clavier (voir [contrôles runtime](./project_structure.md)) pour comparer l'image brute avec et sans anti-aliasing.

Un test synthétique (`test_fxaa`) mesure la réduction du bruit de bord : le seuil d'acceptation est une réduction > 10 % avec la valeur cible autour de 41 %.

## Journal des modifications

| Version | Description |
|---------|-------------|
| 3.11 | Version initiale intégrée — preset qualité maximale |
| Post-sync | Alpha comme source de luminance, correction de la boucle de recherche (parité Intel/NVIDIA) |
