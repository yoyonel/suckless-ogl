# Optimisation des shaders

Ce document décrit les stratégies d'optimisation des shaders selon les modes de build.

## Modes Debug vs Release

Les shaders compilent différemment selon le mode de build via des `#define` de préprocesseur GLSL :

```glsl
#ifdef DEBUG_MODE
    // Visualisations de débogage coûteuses
    debug_output = vec4(normal * 0.5 + 0.5, 1.0);
#endif

#ifdef ENABLE_BLOOM
    // Bloom activé par défaut en Release
    color = apply_bloom(color, u_bloom_intensity);
#endif
```

## Compilation statique des flags

Les flags de compilation sont définis dans CMake et transmis comme `#define` via `glShaderSource` :

```c
// Génération du préambule shader
const char* preamble = generate_shader_preamble(
    .debug_mode = app->debug_mode,
    .enable_bloom = app->settings.bloom_enabled,
    .enable_ssbo = HAS_FEATURE_SSBO,
);
```

Cette approche élimine les branches conditionnelles du shader au runtime.

## Uniformes conditionnels

Pour les fonctionnalités activées/désactivées, préférer les `#define` aux branches uniform :

```glsl
// ❌ Branche sur uniform — le GPU exécute les deux chemins (divergence)
if (u_enable_fog > 0.5) {
    color = apply_fog(color, depth);
}

// ✅ Branche statique — compilée à zéro overhead si désactivée
#ifdef ENABLE_FOG
    color = apply_fog(color, depth);
#endif
```

## Spécialisation par matériau

Plutôt qu'un méga-shader avec tous les effets, le projet compile des variants spécialisés :

| Variant | Defines actifs | Cible |
|---------|---------------|-------|
| `pbr_basic` | Aucun extra | GPU intégrés, mobile |
| `pbr_standard` | `ENABLE_BLOOM`, `ENABLE_FOG` | GPU desktop (défaut) |
| `pbr_ultra` | Tout activé | GPU haut de gamme |

## Analyse des performances

Pour mesurer l'impact d'un bloc shader :

```bash
# Comparer les variantes
just build-release
just bench-shader-variants
```

Le benchmark génère un rapport dans `build/shader_perf_report.txt`.

## Voir aussi

- [shader-cross-gpu-compatibility.md](./shader-cross-gpu-compatibility.md) — Compatibilité cross-GPU
- [postprocess_ubo_architecture.md](./postprocess_ubo_architecture.md) — Architecture des paramètres
- [gpu_profiling.md](./gpu_profiling.md) — Mesure des temps de passe shader
