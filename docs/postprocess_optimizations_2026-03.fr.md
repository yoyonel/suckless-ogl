# Analyse Technique : Optimisations Post-Process (Mars 2026)

Cette documentation détaille les pistes d'optimisation GPU pour le pipeline de post-processing de `suckless-ogl`, basées sur le passage des calculs vers les **Compute Shaders**.

## Objectifs Généraux

1. **Réduction de l'overhead CPU** : Supprimer les changements de Framebuffer (FBO) et les draw calls multiples.

2. **Maximisation de l'occupation GPU** : Utiliser le parallélisme massif des unités de calcul (EUs/CUs) via les Compute Shaders.

3. **Réduction des barrières de synchronisation** : Minimiser les attentes entre les passes.

---

## Partie 1 : Auto-Exposure (Calcul de Luminance)

### Concept

Remplacer la passe de rasterisation (Fragment Shader sur un quad 64x64) par un Compute Shader traitant la texture de scène.

### Points Critiques (Leçons Apprises)

Pour conserver un rendu **ISO avec master**, le Compute Shader doit impérativement répliquer la logique physique :

- **Échantillonnage 4x4** : Ne pas se contenter d'un `texture()` au centre. Il faut moyenner un bloc de pixels (box filter) pour capturer les pics de lumière.

- **Seuil d'exclusion (0.05)** : Ignorer les pixels dont la luminance est inférieure à 0.05. Sans cela, le ciel noir ou les ombres profondes tirent la moyenne vers le bas, provoquant une surexposition massive.

- **Valeur Sentinelle (-100.0)** : Si un bloc est entièrement noir, il doit être marqué pour être ignoré par l'étape d'adaptation.

### Étapes d'implémentation

1. **Shader** : Créer `shaders/lum_downsample.comp` avec une boucle de sampling 4x4.

2. **Texture** : Passer `downsample_tex` en format `R32F` pour le stockage d'images (image2D).

3. **C Code** : Supprimer `downsample_fbo`. Remplacer `glDrawArrays` par `glDispatchCompute(8, 8, 1)`.

4. **Barrière** : Ajouter `glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)` avant l'étape d'adaptation.

---

## Partie 2 : Bloom (Downsampling Single-Pass)

### Concept

Remplacer les 5 à 6 passes de Downsampling successives par un seul dispatch Compute Shader (similaire au _Single Pass Downsampler_ d'AMD).

### Détails Techniques

- **Hiérarchie de Mips** : Utiliser `glBindImageTexture` pour lier plusieurs niveaux de mipmaps (1 à 4) simultanément.

- **Parallélisme** : Chaque groupe de travail (8x8) traite une zone et écrit dans les mips correspondantes.

- **Format** : Utiliser `R11F_G11F_B10F` pour un stockage HDR compact et performant.

### Étapes d'implémentation

1. **Shader** : Créer `shaders/bloom_downsample.comp`.

2. **C Code** : Modifier `fx_bloom_init` pour configurer les textures de mips avec l'accès image.

3. **Render** : Remplacer la boucle de rendu fragment par un dispatch unique. Conserver le mode Raster pour l'Upsampling (qui bénéficie du blending hardware `GL_ONE, GL_ONE`).

---

## Partie 3 : Gestion des Ressources & RAII

### Concept

Fiabiliser la suppression des ressources lors du redémarrage du moteur ou du changement de résolution.

### Recommandations

- **Macro `SHADER_SAFE_DESTROY`** : Utiliser systématiquement une macro vérifiant la nullité avant `shader_destroy`.

- **Nettoyage FBO** : S'assurer que les textures attachées aux FBO sont bien libérées _après_ les FBO pour éviter les dangling pointers dans le driver.

---

## Partie 4 : Validation et Métriques

### Méthodologie de test

1. **Parité Visuelle** : Utiliser `tests/test_visual_fx.c` pour comparer, pixel à pixel, le rendu Raster et le rendu Compute. Toute différence de luminance moyenne > 1% doit être traitée comme un bug.

2. **Benchmarking** : Utiliser `apitrace` pour vérifier la disparition des "bubbles" dans le pipeline (zones où le GPU attend le CPU).

3. **Watchdog** : Pour les tests sous Wine, implémenter un timeout de sortie si l'application ignore le signal `Escape` suite à une désynchronisation X11.

---

_Analyse réalisée le 13 Mars 2026._
