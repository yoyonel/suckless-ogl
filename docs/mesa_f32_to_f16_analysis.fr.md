# Analyse Mesa : Conversion F32 vers F16

**Date** : 2026-02-01
**Composant** : Mesa OpenGL — Pipeline `glTexSubImage2D`

## Contexte

Ce document analyse la chaîne d'appels complète de Mesa lors du traitement de `glTexSubImage2D` avec un format source `GL_FLOAT` et un format interne `GL_RGBA16F`. Objectif : comprendre pourquoi la conversion F32→F16 n'est pas vectorisée sur le chemin critique.

## Chaîne d'appels Mesa

```
glTexSubImage2D(GL_FLOAT → GL_RGBA16F)
    └─ _mesa_TexSubImage2D()
        └─ texsubimage()
            └─ store_texsubimage()
                └─ _mesa_store_texsubimage()
                    └─ _mesa_texstore_rgba16f()
                        └─ SWIZZLE_CONVERT()   ← Macro scalaire
```

La macro `SWIZZLE_CONVERT` dans `src/mesa/main/texstore.c` effectue la conversion pixel par pixel avec `_mesa_float_to_half()`.

## La macro SWIZZLE_CONVERT

```c
#define SWIZZLE_CONVERT(dst_type, src_type, conv)          \
    for (i = 0; i < n; i++) {                               \
        for (j = 0; j < num_dst_channels; j++) {            \
            dst[i * dst_stride + j] =                       \
                conv(src[i * src_stride + swizzle[j]]);     \
        }                                                   \
    }
```

La fonction `conv` est `_mesa_float_to_half()`, une conversion scalaire utilisant des opérations flottantes en virgule fixe standard.

## Pourquoi pas de vectorisation SIMD ?

Analyse via `git blame` et l'historique des commits Mesa :

1. **Complexité du swizzle** : Le tableau `swizzle[]` est dynamique (dépend du format). Les compilateurs ne peuvent pas vectoriser automatiquement les accès à indices non contigus.

2. **Dépendance de chemin** : `_mesa_float_to_half()` utilise des branches conditionnelles (NaN, Inf, dénormals), incompatibles avec l'instruction `_mm256_cvtps_ph` qui suppose des valeurs normalisées.

3. **Historique de maintenance** : Mesa privilégie la portabilité et la lisibilité sur l'optimisation SIMD pour le chemin générique. Les optimisations SIMD sont dans les drivers spécialisés (Intel ANV, RADV).

## Comparaison avec l'implémentation SIMD du projet

Pour les uploads de textures HDR 4K, le projet contourne Mesa avec une conversion SIMD directe :

```c
// Utilisation de F16C (AVX2) pour la conversion batch
#include <immintrin.h>

void convert_f32_to_f16_avx2(const float* src, uint16_t* dst, size_t count) {
    size_t i = 0;
    for (; i + 8 <= count; i += 8) {
        __m256 v = _mm256_loadu_ps(src + i);
        __m128i h = _mm256_cvtps_ph(v, _MM_FROUND_TO_NEAREST_INT);
        _mm_storeu_si128((__m128i*)(dst + i), h);
    }
    // Traitement du reste scalaire
    for (; i < count; i++) {
        dst[i] = _mesa_float_to_half(src[i]);
    }
}
```

Résultat : **3.2× plus rapide** que le chemin Mesa pour les textures RGBA16F 4K (voir [simd_optimization.md](./simd_optimization.md)).

## Voir aussi

- [simd_optimization.md](./simd_optimization.md) — Optimisation AVX2 F16C
- [async_pbo.md](./async_pbo.md) — Upload de textures sans blocage
- [texture_optimization.md](./texture_optimization.md) — Autres optimisations de textures
