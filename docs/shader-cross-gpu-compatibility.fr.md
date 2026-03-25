# Compatibilité des shaders entre GPU

Ce document établit les directives pour écrire des shaders GLSL qui fonctionnent de manière cohérente sur Intel, NVIDIA et AMD.

## Règle n°1 : Ne pas utiliser `dFdx`/`dFdy` pour la logique critique

Les dérivées écran-espace ont des implémentations différentes selon les fournisseurs :

| Fournisseur | Précision | Comportement aux bords |
|-------------|----------|----------------------|
| Intel | Différences finies 2×2 | Stable, conservateur |
| NVIDIA | Unités hardware optimisées | Peut diverger aux bords primitifs |
| AMD | Selon le mode d'exécution | Variable selon RDNA vs GCN |

**Guideline** : Utiliser `dFdx`/`dFdy` uniquement pour les effets visuels non-critiques (bump mapping, LOD de texture). Jamais pour des seuils, des décisions de branchement, ou des calculs affectant la cohérence de sortie.

```glsl
// ✅ Acceptable — effet visuel, imprécision invisible
float lod = log2(length(dFdx(uv)) + length(dFdy(uv)));

// ❌ Problématique — affecte le résultat final
float max_variation = max(abs(dFdx(roughness)), abs(dFdy(roughness)));
float clamped = clamp(pow(max_variation, 0.1), 0.0, 1.0); // Diverge sur NVIDIA
```

## Règle n°2 : Précalculer les valeurs critiques

Pour les calculs utilisés dans plusieurs endroits, calculer en amont et transmettre via `varying` :

```glsl
// Vertex shader
out float v_luminance;

void main() {
    v_luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
}

// Fragment shader — utiliser la valeur interpolée, stable sur tous les GPU
in float v_luminance;
```

## Règle n°3 : Qualificateurs de précision explicites

Ne pas se fier aux précisions par défaut, qui varient selon les GPU mobiles et intégrés :

```glsl
// ✅ Explicite — identique sur tous les GPU
highp float depth = gl_FragCoord.z;
mediump vec3 normal = normalize(v_normal);
lowp vec4 albedo = texture(u_albedo, v_uv);
```

## Pièges courants

### Division par zéro potentielle

```glsl
// ❌ Peut être 0 sur certains GPU (résultat de signe)
float s = sign(value) * something;

// ✅ Version sûre
float s = (value >= 0.0 ? 1.0 : -1.0) * something;
```

### NaN et Inf

```glsl
// ❌ Peut produire NaN si length = 0
vec3 normalized = v / length(v);

// ✅ Protection explicite
vec3 normalized = length(v) > 1e-6 ? v / length(v) : vec3(0.0);
```

### Ordre d'évaluation des opérations

GLSL ne garantit pas l'ordre d'évaluation des sous-expressions. Ne pas écrire :

```glsl
// ❌ Ordre non défini
float a = (x = compute_x(), y = compute_y(x), y * 2.0);

// ✅ Explicite
float x_val = compute_x();
float y_val = compute_y(x_val);
float a = y_val * 2.0;
```

## Validation cross-GPU

Pour chaque nouveau shader :

1. Tester sur Intel HD (comportement de référence)
2. Tester sur NVIDIA (vérification de la synchronisation)
3. Comparer visuellement avec les captures de test

```bash
# Tests de régression visuels
just test-visual-regression
```

## Voir aussi

- [gpu-rendering-synchronization.md](./gpu-rendering-synchronization.md) — Cas concret Intel vs NVIDIA
- [specular_aa.md](./specular_aa.md) — Anti-aliasing spéculaire cross-GPU
- [shader_optimization.md](./shader_optimization.md) — Optimisations conditionnelles
