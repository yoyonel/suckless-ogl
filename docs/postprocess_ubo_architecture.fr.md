# Architecture UBO de post-traitement

Ce document décrit l'architecture des Uniform Buffer Objects (UBOs) pour les effets de post-traitement.

## Vue d'ensemble

Tous les paramètres de post-traitement sont regroupés dans un seul UBO `PostProcessUBO` pour minimiser les appels de liaison et améliorer la cohérence entre les passes de shaders.

## Layout std140

Le layout `std140` garantit une correspondance exacte entre la structure C et le bloc GLSL, mais impose des règles d'alignement strictes.

### Le piège « Array vs Scalar »

En `std140`, chaque élément d'un tableau est aligné sur **16 octets** (la taille d'un `vec4`), même si c'est un `float` :

```glsl
// GLSL std140 — ATTENTION
layout(std140) uniform PostProcess {
    float values[4];  // Occupe 4 × 16 = 64 octets (!), pas 4 × 4 = 16
    vec4  colors[4];  // Occupe 4 × 16 = 64 octets (normal)
};
```

### Structure C correspondante

```c
// Structure C correcte pour std140 avec tableau de floats
typedef struct {
    // Chaque float doit être dans un vec4 pour l'alignement std140
    vec4 values; // 4 floats dans un vec4 au lieu d'un tableau float[4]
    vec4 colors[4];
} PostProcessUBO;
```

**Règle** : En `std140`, préférer les `vec4` aux tableaux de scalaires.

## Bloc GLSL

```glsl
layout(std140, binding = 0) uniform PostProcess {
    // Exposition
    float u_ev_manual;
    float u_auto_exposure;
    float u_adaptation_speed;
    float _pad0;

    // Tonemapping
    float u_tonemap_mode;
    float u_bloom_intensity;
    float u_bloom_threshold;
    float _pad1;

    // Effets cinématiques
    float u_fog_density;
    float u_grain_strength;
    float u_vignette_strength;
    float _pad2;

    vec4  u_fog_color;
} pp;
```

## Ajout d'un nouveau paramètre

Pour ajouter un paramètre au système de post-traitement :

### 1. Mettre à jour la structure C (`include/postprocess.h`)

```c
typedef struct {
    // ... paramètres existants ...

    // Nouveau paramètre
    float new_effect_strength;
    float _pad_new[3]; // Aligner sur 16 octets si nécessaire
} PostProcessUBO;
```

### 2. Mettre à jour le bloc GLSL (`shaders/postprocess/common.glsl`)

```glsl
layout(std140, binding = 0) uniform PostProcess {
    // ... uniforms existants ...
    float u_new_effect_strength;
    float _pad_new[3];
} pp;
```

### 3. Utiliser dans le shader

```glsl
// Utiliser via le préfixe pp.
float effect = pp.u_new_effect_strength;
```

### 4. Mettre à jour depuis le CPU

```c
app->postprocess.ubo_data.new_effect_strength = value;
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PostProcessUBO),
                &app->postprocess.ubo_data);
```

## Validation de l'alignement

Un test compile-time vérifie que la structure C correspond exactement au layout GLSL attendu :

```c
_Static_assert(offsetof(PostProcessUBO, fog_color) == 48,
               "PostProcessUBO layout mismatch");
_Static_assert(sizeof(PostProcessUBO) % 16 == 0,
               "PostProcessUBO size must be multiple of 16");
```

## Voir aussi

- [exposure_analysis.md](./exposure_analysis.md) — Paramètres d'exposition
- [cinematic_rendering.md](./cinematic_rendering.md) — Paramètres cinématiques
- [shader_optimization.md](./shader_optimization.md) — Modes de compilation des shaders
