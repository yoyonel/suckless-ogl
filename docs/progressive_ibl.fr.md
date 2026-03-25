# IBL Progressif — Génération incrémentielle

Ce document décrit l'architecture de génération progressive (incrémentielle par image) de la carte spéculaire préfiltrée IBL.

## Problème initial

La génération complète de la carte spéculaire préfiltrée (Prefiltered Specular Map) pour un HDR 4K en une seule passe prenait ~350 ms sur GPU intégré, causant un gel visible de l'application.

## Solution : Découpe temporelle

Au lieu de générer tous les niveaux de mipmap en une seule frame, on en génère un ou quelques-uns par image.

### Stratégie de découpe

```c
typedef struct {
    int current_mip;      // Niveau actuellement en cours de génération
    int total_mips;       // Nombre total de niveaux
    bool in_progress;     // Génération en cours
} IBLProgressState;
```

Dans la boucle principale :

```c
if (ibl_progress.in_progress) {
    // Générer 1 niveau par image
    generate_specular_mip(&ibl, ibl_progress.current_mip);
    ibl_progress.current_mip++;

    if (ibl_progress.current_mip >= ibl_progress.total_mips) {
        ibl_progress.in_progress = false;
    }
}
```

### Découpe par tranche Y (`u_max_y_slice`)

Pour les niveaux de mip larges (mip 0, mip 1), même une seule passe peut dépasser le budget. Le shader accepte un paramètre `u_max_y_slice` pour traiter seulement une bande horizontale :

```glsl
// Uniforme de contexte
uniform int u_max_y_slice;

void main() {
    ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
    if (texel.y >= u_max_y_slice) return; // Sauter les texels hors de la tranche
    // ... calcul ...
}
```

### Protection contre le chevauchement

Un mécanisme évite de démarrer une nouvelle génération avant que la précédente soit terminée :

```c
// Vérifier la clôture de la passe précédente
if (ibl.prev_fence != NULL) {
    GLenum status = glClientWaitSync(ibl.prev_fence, 0, 0);
    if (status == GL_TIMEOUT_EXPIRED) return; // Pas encore terminée
    glDeleteSync(ibl.prev_fence);
}

// Lancer la passe suivante
dispatch_mip_pass(...);
ibl.prev_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
```

## Optimisation de la barrière différée

La barrière `glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT)` n'est émise qu'une seule fois après la génération complète de tous les mips, plutôt qu'après chaque mip :

```c
// Avant (une barrière par mip — sous-optimal)
for (int mip = 0; mip < total_mips; mip++) {
    generate_mip(mip);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT); // ← Trop fréquent
}

// Après (une seule barrière finale)
for (int mip = 0; mip < total_mips; mip++) {
    generate_mip(mip);
}
glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT); // ← Une seule fois
```

## Résultats de benchmark

| Configuration | Gel visible | Temps total génération |
|-----------|-----------|-----------------------|
| Génération monolithique | ~350 ms | 350 ms |
| Génération progressive (1 mip/frame) | **0 ms** | ~10 frames × 35 ms |

## Voir aussi

- [ibl_architecture_ideas.md](./ibl_architecture_ideas.md) — Idées d'optimisations futures
- [async_loader.md](./async_loader.md) — Intégration avec le chargeur HDR
- [synchronization_overview.md](./synchronization_overview.md) — Synchronisation des ressources GPU
