# Nettoyage des erreurs OpenGL

Ce document dresse la liste des erreurs et avertissements OpenGL identifiés et corrigés dans le projet.

## Erreurs de stabilité

### `GL_INVALID_OPERATION` (0x0502)

| Source | Message | Correctif |
|--------|---------|---------|
| `glUniform*` | Pas de programme actif | Appeler `glUseProgram` avant tout `glUniform*` |
| `glObjectLabel` | Ressource non initialisée | Appeler après allocation compète (incluant mipmaps) |
| `glMapBuffer` | Tampon déjà mappé | Vérifier `glUnmapBuffer` avant un nouveau `glMapBuffer` |

### `GL_INVALID_ENUM` (0x0500)

| Source | Message | Correctif |
|--------|---------|---------|
| `glTexParameter` | Paramètre invalide | Vérifier la compatibilité du format cible |
| `glBlendFunc` | Facteur invalide | Utiliser les constantes `GL_SRC_ALPHA`/`GL_ONE_MINUS_SRC_ALPHA` |

### `GL_INVALID_VALUE` (0x0501)

| Source | Message | Correctif |
|--------|---------|---------|
| `glTexStorage2D` | Dimensions hors plage | Limiter à `GL_MAX_TEXTURE_SIZE` |
| `glVertexAttribPointer` | Stride ou offset négatif | S'assurer que les calculs sont non signés |

## Avertissements de performance

### `0x20072` — Mapping d'un tampon occupé

**Signification** : Le CPU attend que le GPU libère un tampon avant de pouvoir le mapper.

**Correctif** : Utiliser le double tampon + clôtures explicites `GLsync`. Voir [async_pbo.md](./async_pbo.md).

```c
// Au lieu de (bloquant)
void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

// Utiliser (non bloquant avec double tampon)
GLenum result = glClientWaitSync(fence[!active_idx], GL_SYNC_FLUSH_COMMANDS_BIT, 0);
if (result != GL_TIMEOUT_EXPIRED) {
    void* ptr = glMapBuffer(/* ... */);
}
```

### `0x20084` — Format de texture non natif

**Signification** : Le format demandé nécessite une conversion interne par le pilote.

**Exemples problématiques** :
- Upload de `GL_UNSIGNED_BYTE` dans un format interne `GL_RGBA16F` → conversion automatique coûteuse
- Utilisation de `GL_RGB` (3 canaux) au lieu de `GL_RGBA` (4 canaux) sur certains GPU

**Correctif** : Aligner le format source avec le format interne :

```c
// Format interne GL_RGBA16F → utiliser GL_HALF_FLOAT comme type source
glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                GL_RGBA, GL_HALF_FLOAT, data_f16);
```

### `0x20092` — Écriture dans un tampon partagé

**Signification** : Le tampon est utilisé simultanément par le CPU et le GPU.

**Correctif** : Utiliser `GL_MAP_UNSYNCHRONIZED_BIT` uniquement quand on peut garantir l'absence de conflit, ou utiliser des clôtures explicites.

## Avertissements de dépréciation

Ces API sont légales mais déconseillées dans les contextes Core Profile :

| API | Problème | Alternative |
|-----|---------|------------|
| `glBegin`/`glEnd` | Rendu immédiat (OpenGL 1.x) | VAO + VBO |
| `glPushMatrix`/`glPopMatrix` | Pile de matrices fixe | Matrices CPU + UBO |
| `glGenTextures` sans `glBindTexture` | État implicite | `glCreateTextures` (DSA) |

## Processus de nettoyage

Pour identifier les erreurs GL restantes :

```bash
# Build avec vérifications synchrones (chaque appel GL vérifié immédiatement)
just build-sync
just run-sync 2>&1 | grep -E "GL_|ERROR|WARN"
```

## Voir aussi

- [debugging.md](./debugging.md) — Sortie de débogage GL
- [async_pbo.md](./async_pbo.md) — Uploads sans synchronisation implicite
- [nvidia_optimizations.md](./nvidia_optimizations.md) — Problèmes spécifiques NVIDIA
