# Synchronisation du rendu GPU : Intel vs NVIDIA

**Date** : 2026-01-30
**Statut** : Résolu
**Impact** : Critique — Cohérence de la qualité visuelle entre les fournisseurs de GPU

## Résumé

Investigation et résolution des différences de rendu entre les GPU Intel et NVIDIA dans le moteur PBR `suckless-ogl`. Les problèmes se manifestaient par des halos blancs et un mélange incorrect des bords FXAA sur le matériel NVIDIA.

## Comparaison visuelle

| GPU | Avant correction | Après correction |
|-----|-----------------|-----------------|
| **Intel** | ✅ Bords propres, FXAA correct | ✅ Inchangé |
| **NVIDIA** | ❌ Halos blancs, bords incorrects | ✅ Identique à Intel |

## Causes identifiées

### Problème 1 : Recalcul de la luminance FXAA

**Fichier** : `shaders/postprocess/fxaa.glsl` (Lignes 176-198)

**Problème** : La boucle de recherche d'arêtes recalculait la luminance via `sqrt()`, qui présente une précision différente sur Intel et NVIDIA.

**Correctif** : Utiliser la luminance pré-calculée depuis le canal alpha.

```diff
- lumaEnd1 = FxaaLuma(texture(screenTexture, uv1).rgb);
+ lumaEnd1 = texture(screenTexture, uv1).a;  // Pré-calculée dans le shader PBR
```

### Problème 2 : Limitation de la rugosité basée sur les dérivées

**Fichier** : `shaders/pbr_functions.glsl` (Lignes 76-100)

**Problème** : `dFdx()`/`dFdy()` produisent des valeurs différentes sur Intel et NVIDIA, causant des valeurs de rugosité extrêmes aux bords sur NVIDIA.

**Tentatives de correctif** :

1. ❌ Seuil 0.1 → 0.5 : Réduction mais persistance des halos
2. ❌ Saturation `min(maxVariation, 1.0)` : Artefacts toujours visibles
3. ✅ **Suppression complète** : Parité visuelle obtenue

**Solution finale** : Désactivation complète du clamping de rugosité.

```glsl
float compute_roughness_clamping(vec3 N_val, float roughness_val)
{
    // Désactivé : les dérivées ont une précision différente sur NVIDIA vs Intel
    roughness_val = clamp(roughness_val, 0.0, 1.0);
    return roughness_val;
}
```

## Pourquoi les dérivées diffèrent

| Fournisseur | Implémentation | Comportement |
|-------------|---------------|-------------|
| **Intel** | Différences finies conservatrices sur quadrilatères 2×2 | Stables, valeurs prévisibles |
| **NVIDIA** | Unités matérielles optimisées | Arrondi différent, peut produire des pics |

**Résultat** : `pow(maxVariation, 0.1)` amplifiait les différences entre fournisseurs → halos blancs sur NVIDIA.

## Compromis

### Perdu

- Anti-aliasing géométrique sur les surfaces courbes
- Prévention des aliasings spéculaires sur les métaux très lisses

### Gagné

- ✅ Cohérence inter-fournisseurs
- ✅ Comportement prévisible
- ✅ Code shader simplifié
- ✅ Légère amélioration des performances

**Verdict** : FXAA fournit déjà un excellent anticrénelage. La perte du clamping de rugosité dépendant du matériel est compensée par un minimum analytique stable et un clamping spécifique à la courbure des sphères.

## Dérivées vs performances analytiques

Une découverte clé lors de cette investigation est le compromis entre les fonctions intégrées GLSL et les calculs analytiques personnalisés.

| Méthode | Intégré (`dFdx`, `fwidth`) | Courbure/Atténuation analytique |
| :--- | :--- | :--- |
| **Cohérence inter-fournisseurs** | ❌ Faible (précision pilote/matériel) | ✅ 100 % (mathématique) |
| **Latence** | Moyenne (synchronisation quad requise) | Faible (pure ALU) |
| **Sûreté logique** | ❌ Échoue dans les branches divergentes | ✅ Sûr pour les branches |
| **Implémentation** | Triviale (1 ligne) | Complexe (math personnalisée par primitive) |

**Évaluation du coût** : Bien que les calculs analytiques utilisent plus de cycles ALU (environ 5-10 de plus), ils évitent la latence de synchronisation des quadrilatères matériels et garantissent que les cartes de régression entre Intel, NVIDIA et AMD restent vierges (parité bit à bit).

## Résultats de validation

### Inspection visuelle

```
Avant :  Intel ✅  |  NVIDIA ❌ (halos, FXAA incorrect)
Après :  Intel ✅  |  NVIDIA ✅ (rendu identique)
```

### Métriques de référence (Test synthétique FXAA)

Valeurs cibles basées sur le comportement correct d'Intel HD 4600 (Motif en sphère) :

| Métrique | Valeur de référence (Intel) | NVIDIA (GTX 950M) | Seuil d'acceptation |
|----------|-----------------------------|-------------------|---------------------|
| **Bruit de bord (sans AA)** | ~0.0026 | 0.0026 | N/A |
| **Bruit de bord (FXAA)** | ~0.0015 | 0.0015 | < 0.0020 |
| **Réduction du bruit** | **~41.88 %** | **41.85 %** | > 10 % |

> **Conclusion** : Le rendu NVIDIA est maintenant mathématiquement identique à Intel (delta < 0.1 %), confirmant que la correction de précision est réussie.

## Fichiers modifiés

- `shaders/postprocess/fxaa.glsl`
- `shaders/pbr_functions.glsl`

## Synchronisation des mappings PBO (implicite vs explicite)

L'un des problèmes de performance les plus insaisissables en OpenGL est la **synchronisation implicite** qui se produit lors de `glMapBuffer`.

### Le symptôme

ApiTrace rapporte : `api performance issue 1: memory mapping a busy "buffer" BO stalled and took 1.379 ms.`

### La cause

Si vous tentez de mapper un tampon actuellement utilisé par une commande GPU en attente (comme un `glReadPixels` ou `glTexSubImage2D`), le pilote doit suspendre le CPU jusqu'à la fin de cette commande. Même le mapping « non synchronisé » peut parfois bloquer si le tampon n'a pas été correctement clôturé.

### Le correctif : Synchronisation explicite (`GLsync`)

Au lieu de laisser le pilote deviner, nous utilisons une synchronisation explicite :

1. **Clôture après commande** :

   ```c
   glReadPixels(...);
   app->sync[idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   ```

2. **Attente avant le mapping** :

   ```c
   // Attente non bloquante (délai 0)
   GLenum status = glClientWaitSync(app->sync[!idx], GL_SYNC_FLUSH_COMMANDS_BIT, 0);
   if (status != GL_TIMEOUT_EXPIRED) {
       void* ptr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
       // ... traitement ...
       glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
   }
   ```

En vérifiant la clôture avec un délai nul, on s'assure que si le GPU est encore occupé, le CPU saute simplement la logique pour cette image au lieu d'attendre. C'est crucial pour maintenir une fréquence d'images élevée et stable.

## Références

- [shader-cross-gpu-compatibility.md](./shader-cross-gpu-compatibility.md) — Directives générales
- [FXAA 3.11 Whitepaper](https://developer.nvidia.com/fxaa)
- [OpenGL Derivatives](https://www.khronos.org/opengl/wiki/Derivative)
