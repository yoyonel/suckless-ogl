# Optimisation des textures

Ce document décrit les pratiques d'optimisation pour le stockage et l'upload des textures OpenGL.

## Stockage immuable avec `glTexStorage2D`

L'utilisation de `glTexStorage2D` (OpenGL 4.2+) alloue la texture avec une taille et un format fixes, offrant plusieurs avantages over `glTexImage2D` :

```c
// ✅ Stockage immuable — alloue tous les niveaux en une fois
glTexStorage2D(GL_TEXTURE_2D,
               num_mip_levels,    // 1 + floor(log2(max(width, height)))
               GL_RGBA16F,        // Format interne
               width, height);

// ❌ Stockage mutable — alloue niveau par niveau, potentiellement re-alloué
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
             GL_RGBA, GL_FLOAT, NULL);
```

### Avantages du stockage immuable

1. **Performance pilote** : L'allocation complète dès le début évite les réallocations internes
2. **Validation** : Le pilote peut valider les formats et dimensions une seule fois
3. **Cohérence** : Impossible de modifier accidentellement le format après allocation
4. **NVIDIA** : Réduit les warnings de validation sur certains pilotes

## Correction de l'alignement d'unpack

Par défaut, OpenGL suppose un alignement de 4 octets pour les lignes de texture. Pour les textures dont la largeur n'est pas un multiple de 4, cela cause des décalages :

```c
// Vérifier si l'alignement par défaut (4) est correct
uint32_t row_bytes = width * channels; // ex: 3 canaux × largeur impair

if (row_bytes % 4 != 0) {
    // Forcer l'alignement à 1 octet
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    upload_texture(...);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // Restaurer
}
```

Cas courants nécessitant l'alignement à 1 :
- Textures RGB (3 canaux) de largeur impaire
- Textures de glyphes de fontes (1 canal, largeur quelconque)
- Données compressées non-alignées

## Diagramme de layout mémoire

```
Texture 5×2 pixels, RGB (3 octets/pixel), alignement 4 :

Ligne 0 : [R G B] [R G B] [R G B] [R G B] [R G B] [PAD]
           pixel0  pixel1  pixel2  pixel3  pixel4   (1 octet de padding)

Ligne 1 : [R G B] [R G B] [R G B] [R G B] [R G B] [PAD]
           pixel5  pixel6  pixel7  pixel8  pixel9

14 octets + 1 padding = 15 → arrondi à 16 (multiple de 4)
```

Avec `GL_UNPACK_ALIGNMENT = 1` :
```
Ligne 0 : [R G B] [R G B] ... [R G B]  ← 15 octets, pas de padding
Ligne 1 : [R G B] [R G B] ... [R G B]  ← 15 octets
```

## Upload via PBO vs transfert direct

| Méthode | Latence | Cas d'usage |
|---------|---------|-------------|
| Direct (`glTexSubImage2D`) | Bloquant | Init unique, petites textures |
| PBO simple | Non bloquant | Uploads fréquents |
| PBO double tampon persistant | Non bloquant optimal | Textures HDR large, streaming |

Pour les textures HDR 4K, toujours utiliser le PBO double tampon persistant (voir [async_pbo.md](./async_pbo.md)).

## Formats recommandés

| Contenu | Format interne | Type source | Notes |
|---------|---------------|------------|-------|
| HDR environment | `GL_RGBA16F` | `GL_HALF_FLOAT` | Après conversion SIMD |
| Albédo sRGB | `GL_SRGB8_ALPHA8` | `GL_UNSIGNED_BYTE` | Conversion gamma auto |
| Normal map | `GL_RGB8_SNORM` | `GL_BYTE` | Signé pour les normales |
| Roughness/Metal | `GL_RG8` | `GL_UNSIGNED_BYTE` | 2 canaux suffisent |
| Depth buffer | `GL_DEPTH_COMPONENT32F` | — | 32 bits pour la précision |

## Voir aussi

- [async_pbo.md](./async_pbo.md) — PBOs persistants double tampon
- [simd_optimization.md](./simd_optimization.md) — Conversion F32→F16 optimisée
- [opengl_cleanup.md](./opengl_cleanup.md) — Avertissements de format `0x20084`
