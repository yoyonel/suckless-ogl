# PBOs asynchrones — Double tampon persistant

Ce document décrit l'architecture des Pixel Buffer Objects (PBOs) à double tampon persistant utilisée pour les transferts GPU sans synchronisation.

## Contexte et objectifs

L'objectif est de transférer des données volumineux (textures HDR 4K, readbacks de pixels) entre le CPU et le GPU **sans jamais bloquer la boucle de rendu**. Cela implique :

- Pas d'orphelinage de tampon (`glBufferData` sur un tampon actif)
- Pas d'attente de synchronisation CPU/GPU
- Zéro appel à `glGetError` dans le chemin critique

## Double tampon persistant

### Principe

Deux PBOs sont alloués une fois à l'initialisation avec `GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT`. Ils restent mappés en permanence.

```
Image N-1 : PBO[0] → GPU traite
Image N   : CPU écrit dans PBO[1]
Image N+1 : PBO[1] → GPU traite, CPU écrit dans PBO[0]
```

À aucun moment le même PBO n'est écrit par le CPU et lu par le GPU simultanément.

### Allocation

```c
// Allocation persistante — une seule fois
glGenBuffers(2, pbo);
for (int i = 0; i < 2; i++) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[i]);
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER, HDR_MAX_SIZE,
                    NULL,
                    GL_MAP_WRITE_BIT |
                    GL_MAP_PERSISTENT_BIT |
                    GL_MAP_COHERENT_BIT);
    mapped_ptr[i] = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, HDR_MAX_SIZE,
                                     GL_MAP_WRITE_BIT |
                                     GL_MAP_PERSISTENT_BIT |
                                     GL_MAP_COHERENT_BIT);
}
```

## Initialisation multi-image

Certaines ressources GPU (notamment les cube maps IBL) nécessitent plusieurs images pour être entièrement initialisées. Le protocole garantit que :

1. La ressource est créée avec `glTexStorage2D` (stockage immuable)
2. Chaque face/mip est chargée progressivement via le PBO
3. Un point de synchronisation explicite (`GLsync`) confirme la fin avant utilisation

## Suppression des points de synchronisation

### L'audit `glGetError`

L'audit initial du code révélait des appels `glGetError` après chaque upload, utilisés comme synchronisation implicite. Ces appels ont été supprimés car :

- `glGetError` vide la file de commandes GPU → synchronisation forcée non nécessaire
- Le double tampon garantit déjà qu'aucune course n'est possible
- Les clôtures explicites (`glFenceSync`) couvrent les cas où une synchronisation est réellement requise

### Résultat

```
Avant :  ~1.4 ms de blocage par upload (implicit sync via glGetError)
Après :  < 0.05 ms (transfert asynchrone, aucun blocage)
```

## Phases d'évolution

### Phase 1 — PBO simple
Upload synchrone via `glMapBuffer` + `memcpy`. Simple mais bloquant.

### Phase 2 — PBO double tampon
Alternance entre deux PBOs pour éviter les conflits. Efficace pour les textures régulières.

### Phase 3 — PBO persistant (actuel)
Mappage permanent, cohérence automatique, suppression de `glGetError`. Adapté aux uploads haute fréquence et aux grandes textures HDR.

## Readback (lecture GPU → CPU)

Le même principe s'applique aux readbacks de pixels (histogramme de luminance, débogage) :

```c
// Déclencher le readback (non bloquant)
glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo_read[idx]);
glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, 0);
fence[idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

// Image suivante : lire le résultat si disponible
GLenum status = glClientWaitSync(fence[!idx], GL_SYNC_FLUSH_COMMANDS_BIT, 0);
if (status != GL_TIMEOUT_EXPIRED) {
    void* data = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    process_histogram(data);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
}
```

## Voir aussi

- [async_loader.md](./async_loader.md) — Vue d'ensemble du chargeur HDR
- [gpu-rendering-synchronization.md](./gpu-rendering-synchronization.md) — Synchronisation explicite
- [synchronization_overview.md](./synchronization_overview.md) — Philosophie de synchronisation
