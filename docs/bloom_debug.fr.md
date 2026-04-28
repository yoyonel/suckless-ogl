# Mode de débogage Bloom

Ce document décrit le mode de débogage interactif du post-effet Bloom.

## Vue d'ensemble

Le mode de débogage Bloom permet d'inspecter chaque étape du pipeline bloom ainsi que chaque niveau de mipmap individuellement. Cela facilite le réglage des paramètres et le diagnostic des artefacts visuels.

## Contrôles

| Raccourci | Action |
|-----------|--------|
| `SHIFT+B` | Cycle entre les étapes du pipeline Bloom |
| `ALT+B` | Cycle entre les niveaux de mipmap de l'étape courante |

## Étapes du pipeline

Le pipeline Bloom est composé de plusieurs passes successives que l'on peut inspecter individuellement :

1. **Seuillage** — Extraction des pixels dépassant le seuil de luminance
2. **Blur descendant** — Série de passes de flou avec downsampling progressif
3. **Blur ascendant** — Recombinaison avec upsampling (tent filter)
4. **Composition finale** — Mélange avec l'image originale selon le facteur d'intensité

## Intégration dans la barre d'état

Lors de l'activation du mode de débogage, la **Superposition d'informations principale** (F1) affiche :
- L'étape actuellement visualisée
- Le niveau de mip actif
- Les paramètres courants (seuil, intensité)

## Architecture

Depuis l'introduction du seam `EffectContext`, le module bloom est découplé de `PostProcess` :

- `postprocess.c` construit un `EffectContext` (texture source, dimensions viewport) et appelle `fx_bloom_render(BloomFX*, BloomParams*, EffectContext*)`
- `fx_bloom.c` n'inclut plus `postprocess.h` — il reçoit uniquement ce dont il a besoin via `EffectContext` + `BloomParams`
- Le debug mode fonctionne de manière identique : `debug_step` et `debug_mip` sont des champs de `BloomFX`

## Voir aussi

- [exposure_analysis.md](./exposure_analysis.md) — Interaction entre exposition et bloom
- [postprocess_ubo_architecture.md](./postprocess_ubo_architecture.md) — Architecture des paramètres post-traitement
