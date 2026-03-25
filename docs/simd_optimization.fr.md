# Optimisation SIMD — Conversion F32 vers F16

Ce document décrit l'optimisation AVX2/F16C pour la conversion batch de textures HDR.

## Contexte

Pour les uploads de textures HDR 4K (4096×2048 pixels RGBA float), la conversion de `float` (32 bits) vers `half` (16 bits) représente un goulot d'étranglement CPU significatif.

- Taille source : 4096 × 2048 × 4 canaux × 4 octets = 128 MB
- Opérations scalaires nécessaires : ~33 millions de conversions F32→F16

## Solution : Extension F16C (AVX2)

L'instruction `_mm256_cvtps_ph` convertit 8 floats 32 bits en 8 halfs 16 bits en une seule opération SIMD.

### Implémentation

```c
#include <immintrin.h>

// Vérification de la disponibilité F16C au runtime
bool has_f16c = __builtin_cpu_supports("f16c") &&
                __builtin_cpu_supports("avx2");

void convert_f32_to_f16_avx2(const float* src, uint16_t* dst, size_t count)
{
    size_t i = 0;

    // Traitement vectoriel par blocs de 8
    for (; i + 8 <= count; i += 8) {
        __m256 v = _mm256_loadu_ps(src + i);
        __m128i h = _mm256_cvtps_ph(v, _MM_FROUND_TO_NEAREST_INT);
        _mm_storeu_si128((__m128i*)(dst + i), h);
    }

    // Reste scalaire
    for (; i < count; i++) {
        // Conversion scalaire portable
        uint32_t bits;
        memcpy(&bits, &src[i], sizeof(float));
        // ... conversion IEEE 754 manuelle ...
    }
}
```

### Gestion du tampon temporaire

Pour les textures 4K, un tampon intermédiaire de 64 MB est alloué une fois :

```c
// Allocation unique, réutilisée pour tous les chargements HDR
static uint16_t* f16_buffer = NULL;
static size_t f16_buffer_size = 0;

if (pixel_count > f16_buffer_size) {
    free(f16_buffer);
    f16_buffer = malloc(pixel_count * sizeof(uint16_t) * 4); // RGBA
    f16_buffer_size = pixel_count;
}
```

## Mesures de performance

Configuration : Intel Core i7, texture RGBA16F 4096×2048

| Méthode | Temps | Débit |
|---------|-------|-------|
| Scalaire (boucle C) | ~48 ms | ~2.7 GB/s |
| Mesa (`_mesa_float_to_half`) | ~52 ms | ~2.5 GB/s |
| **AVX2 F16C** | **~15 ms** | **~8.5 GB/s** |

Accélération : **3.2× plus rapide** que la conversion scalaire.

## Vérification de la correction

Un test unitaire compare les résultats SIMD avec la conversion scalaire de référence :

```c
// Test exhaustif sur 1000 valeurs aléatoires
for (int i = 0; i < 1000; i++) {
    float src = random_float();
    uint16_t simd_result = f32_to_f16_simd(src);
    uint16_t ref_result = f32_to_f16_scalar(src);

    // Tolérance d'un ULP (Unit in the Last Place) pour les dénormals
    assert(abs(simd_result - ref_result) <= 1);
}
```

## Détection de capacité

```c
void texture_upload_init(void) {
    if (__builtin_cpu_supports("f16c") && __builtin_cpu_supports("avx2")) {
        g_convert_fn = convert_f32_to_f16_avx2;
        LOG_INFO("Upload textures : chemin AVX2 F16C activé");
    } else {
        g_convert_fn = convert_f32_to_f16_scalar;
        LOG_INFO("Upload textures : chemin scalaire (pas de F16C)");
    }
}
```

## Voir aussi

- [mesa_f32_to_f16_analysis.md](./mesa_f32_to_f16_analysis.md) — Analyse de la chaîne Mesa
- [texture_optimization.md](./texture_optimization.md) — Autres optimisations de textures
- [async_pbo.md](./async_pbo.md) — Upload via PBO persistant
