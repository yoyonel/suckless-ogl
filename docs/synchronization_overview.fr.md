# Vue d'ensemble de la synchronisation

Ce document décrit la philosophie de synchronisation CPU/GPU adoptée dans le projet.

## Philosophie : Non-bloquant par défaut

La règle principale est de **ne jamais bloquer le thread de rendu** pour attendre le GPU, sauf en cas de nécessité absolue.

```
Thread principal (rendu) :   [Frame N] [Frame N+1] [Frame N+2] ...
Thread GPU :                     [...]    [GPU N]   [GPU N+1] [GPU N+2]
```

Le CPU et le GPU doivent travailler en pipeline (décalage d'une image), jamais en séquentiel.

## Transferts PBO — Upload (CPU → GPU)

Pour les uploads de textures (HDR, mises à jour de données) :

1. **Double tampon persistant** : Deux PBOs mappés en permanence
2. **Écriture CPU dans PBO[N % 2]**
3. **GPU lit PBO[(N-1) % 2]** (pas de conflit)
4. **Aucune synchronisation explicite nécessaire** pour les doubles tampons corrects

Documentation détaillée : [async_pbo.md](./async_pbo.md)

## Transferts PBO — Readback (GPU → CPU)

Pour lire des données depuis le GPU (histogramme de luminance, captures d'écran) :

1. Déclencher `glReadPixels` vers un PBO (non bloquant)
2. Créer une clôture : `fence = glFenceSync(...)`
3. Image suivante : vérifier la clôture avec `glClientWaitSync(..., 0)` (timeout 0)
4. Si la clôture est prête : mapper le PBO et lire → zéro attente
5. Si la clôture n'est pas prête : sauter cette image, réessayer au prochain tour

```c
// Vérification non bloquante
GLenum status = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED) {
    // GPU terminé — lire les données
    void* data = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    process_data(data);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
}
// Sinon : ne pas attendre, réessayer au prochain frame
```

## Tâches GPU progressives

Pour les opérations longues (génération IBL, compilation), les découper en tranches d'une image :

```c
// Une seule étape par frame — jamais bloquer pour toutes
if (ibl_state.pending_mip < ibl_state.total_mips) {
    generate_one_mip(ibl_state.pending_mip);
    ibl_state.pending_mip++;
}
```

Documentation détaillée : [progressive_ibl.md](./progressive_ibl.md)

## Gestion des interblocages

Les interblocages documentés et leurs solutions :

| Situation | Cause | Solution |
|-----------|-------|---------|
| Bascule plein écran NVIDIA | `glfwSetWindowMonitor` dans callback | Redimensionnement différé + `glFinish` avant |
| Mutex double-lock async loader | Re-verrouillage depuis le même thread | Détection du thread propriétaire |
| Blocage mapping PBO | Mapping pendant que GPU lit | Double tampon + clôtures `GLsync` |

Références :
- [fullscreen_deadlock.md](./fullscreen_deadlock.md)
- [fix_async_loader_deadlock.md](./fix_async_loader_deadlock.md)
- [gpu-rendering-synchronization.md](./gpu-rendering-synchronization.md)

## Résumé — Quand bloquer, quand ne pas bloquer

| Opération | Approche | Justification |
|-----------|----------|---------------|
| Upload de texture HDR | Non bloquant (PBO double tampon) | Fréquent, latence inacceptable |
| Readback histogramme | Non bloquant (clôture + skip) | OK de manquer une image |
| Changement plein écran | Bloquant (`glFinish`) | Rare, sécurité pilote nécessaire |
| Génération IBL complète | Progressive (1 mip/frame) | Évite le gel visible |
| Capture d'écran | Bloquant acceptable | Action utilisateur, pas en boucle |
