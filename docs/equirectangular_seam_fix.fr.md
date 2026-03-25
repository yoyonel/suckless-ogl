# Correction des coutures equirectangulaires

Ce document décrit le correctif appliqué pour éliminer les artefacts de couture dans le rendu des textures equirectangulaires.

## Problème

Une ligne verticale visible apparaissait sur la skybox à la longitude 180°/−180° (le méridien de wrap). Ce problème affectait particulièrement les environnements avec des gradients subtils (ciel de coucher de soleil, brume atmosphérique).

## Cause

### Mode de répétition incorrect

Le mode de répétition par défaut pour les textures equirectangulaires était `GL_CLAMP_TO_EDGE`. Avec ce mode, les texels aux bords de la texture sont répétés plutôt qu'interpolés, causant une transition brutale à la couture.

### Solution : `GL_REPEAT`

```c
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);   // Axe horizontal
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Axe vertical (pôles)
```

L'axe S (horizontal, longitude) utilise `GL_REPEAT` pour que les bords gauche et droit de la texture se raccordent naturellement. L'axe T (vertical, latitude) garde `GL_CLAMP_TO_EDGE` car les pôles ne se répètent pas.

## Correctif de niveau de mipmap pour l'irradiance

Un second problème affectait la carte d'irradiance : lors du calcul de la convolution d'irradiance, l'accès à la texture avec `texture()` (sélection automatique du mip) pouvait choisir un niveau de mip bas en périphérie, causant un banding.

### Solution : `textureLod` avec niveau 0 explicite

```glsl
// Avant — niveau de mip automatique, peut causer du banding
vec3 env_sample = texture(u_environment, dir_to_uv(sample_dir)).rgb;

// Après — niveau 0 explicite pour la convolution d'irradiance
vec3 env_sample = textureLod(u_environment, dir_to_uv(sample_dir), 0.0).rgb;
```

Le niveau 0 garantit l'utilisation de la résolution maximale de la texture source pour tous les calculs d'irradiance, éliminant les artéfacts de banding.

## Voir aussi

- [cubemap_seam_resolution.md](./cubemap_seam_resolution.md) — Approche globale pour les coutures
- [skybox_rendering.md](./skybox_rendering.md) — Rendu de la skybox
