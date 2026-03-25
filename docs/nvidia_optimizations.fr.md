# Optimisations NVIDIA

Ce document recense les correctifs et optimisations appliqués spécifiquement pour les GPU NVIDIA.

## Correctifs de démarrage

### Allocation de mipmaps avant l'étiquetage

Sur NVIDIA, `vkSetDebugUtilsObjectNameEXT` (ou son équivalent OpenGL `glObjectLabel`) peut causer des avertissements de validation si appelé avant que la texture ait des mipmaps alloués.

**Correctif** : Appeler `glGenerateMipmap` avant `glObjectLabel`.

```c
// Ordre correct pour NVIDIA
glBindTexture(GL_TEXTURE_2D, tex_id);
glTexStorage2D(GL_TEXTURE_2D, num_levels, GL_RGBA16F, width, height);
glGenerateMipmap(GL_TEXTURE_2D);              // ← Avant l'étiquetage
glObjectLabel(GL_TEXTURE, tex_id, -1, name); // ← Après l'allocation
```

### Étiquetage des objets (RenderDoc)

Le driver NVIDIA est strict sur l'étiquetage des ressources : les appels `glObjectLabel` sur des ressources non allouées génèrent des erreurs `GL_INVALID_OPERATION`.

## Optimisations de performance

### Textures factices pour les uniformes non utilisés

Certains shaders NVIDIA génèrent des avertissements si un sampler uniforme est déclaré mais que l'unité de texture correspondante n'est pas liée. Lier des textures factices (1×1 noir) élimine ces avertissements :

```c
// Lier une texture 1×1 noire sur toutes les unités non utilisées
glActiveTexture(GL_TEXTURE0 + unused_unit);
glBindTexture(GL_TEXTURE_2D, app->dummy_texture_black);
```

### Réconciliation VAO

Sur certains pilotes NVIDIA, un VAO non lié lors d'un appel `glDrawArrays` peut causer une chute de performance. S'assurer que le VAO est toujours lié avant le draw :

```c
glBindVertexArray(vao);
glDrawArraysInstanced(GL_TRIANGLES, 0, vertex_count, instance_count);
glBindVertexArray(0); // Délier après pour cohérence
```

### Placement des tampons

Pour les UBOs fréquemment mis à jour, utiliser `GL_DYNAMIC_DRAW` au lieu de `GL_STATIC_DRAW` permet au pilote NVIDIA de choisir le meilleur emplacement mémoire (VRAM vs RAM partagée) :

```c
glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW); // Mis à jour chaque image
glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);  // Rarement modifié
```

## Résultats de validation

Après l'application des correctifs, aucun avertissement de validation Nvidia n'est émis lors d'une session complète de rendu. Les résultats de test visuels sont identiques à Intel (voir [gpu-rendering-synchronization.md](./gpu-rendering-synchronization.md)).

## Voir aussi

- [gpu-rendering-synchronization.md](./gpu-rendering-synchronization.md) — Synchronisation Intel vs NVIDIA
- [opengl_cleanup.md](./opengl_cleanup.md) — Nettoyage général des erreurs OpenGL
- [debugging.md](./debugging.md) — Sortie de débogage GL
