# Optimisation du batching de l'interface utilisateur

Ce document décrit l'architecture d'optimisation du rendu de l'interface clavier par batching.

## Problème initial

Le rendu de l'interface clavier (keyboard overlay) effectuait un draw call séparé par touche, soit 100+ draw calls par frame pour l'affichage complet du clavier.

## Architecture de batching en 3 passes

Le rendu est réorganisé en 3 passes distinctes, chacune traitant toutes les touches du même type en un seul draw call instancié :

### Passe 1 : Touches de base (keycaps)

```c
// Toutes les touches de base en une seule passe
uint32_t num_keys = key_layout_count();
glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, num_keys);
// → 1 draw call pour toutes les touches
```

### Passe 2 : Effets (glow, hover, pressed)

```c
// Toutes les touches actives avec effets
glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, num_active_keys);
// → 1 draw call pour tous les effets
```

### Passe 3 : Étiquettes (labels)

```c
// Tous les glyphes de texte en une seule passe
// Utilise un atlas de texte pour éviter les changements de texture
glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, num_glyphs);
// → 1 draw call pour tout le texte
```

## Résultats de performance

| Métrique | Avant | Après | Gain |
|---------|-------|-------|------|
| Draw calls GPU | ~100-150 | 3 | **33×** |
| Temps GPU (Intel HD, 1080p) | ~14 ms | ~1 ms | **14×** |
| Temps CPU (préparation) | ~0.8 ms | ~0.2 ms | 4× |

## Structure des données d'instance

Chaque touche contribue une entrée dans le buffer d'instances SSBO :

```c
typedef struct {
    vec4  position_size;  // xy = position écran, zw = taille
    vec4  uv_offset;      // UV dans l'atlas de textures
    vec4  color_alpha;    // Couleur de teinte + opacité
    float glow_intensity; // Intensité du glow SDF
    float _pad[3];        // Alignement std430
} KeyInstanceData;
```

L'ensemble du buffer est mis à jour en une seule fois avant les draw calls.

## Voir aussi

- [keyboard_system.md](./keyboard_system.md) — Système de raccourcis et overlay clavier
- [ui_visual_parameters.md](./ui_visual_parameters.md) — Paramètres visuels de l'interface
- [raii_cleanup_guide.md](./raii_cleanup_guide.md) — Macros RAII utilisées dans le rendu UI
