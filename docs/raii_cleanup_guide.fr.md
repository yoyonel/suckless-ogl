# Guide RAII en C

Ce document décrit l'utilisation du pattern RAII (Resource Acquisition Is Initialization) en C via les attributs GCC/Clang.

## Mécanisme : `__attribute__((cleanup))`

GCC et Clang supportent l'attribut `cleanup` qui appelle automatiquement une fonction de nettoyage lorsqu'une variable sort de portée :

```c
// Déclaration du destructeur
static void cleanup_timer(HybridTimer** t) {
    if (*t) hybrid_timer_stop(*t);
}

// Utilisation avec RAII
void ma_fonction(void) {
    HybridTimer* timer __attribute__((cleanup(cleanup_timer))) =
        hybrid_timer_start("Ma Fonction");

    // ... code ...

} // ← timer est automatiquement stoppé ici, même en cas de return anticipé
```

## Macros RAII du projet

### HYBRID_FUNC_TIMER

Mesure le temps d'exécution d'une fonction CPU complète :

```c
void render_frame(App* app) {
    HYBRID_FUNC_TIMER; // ← Démarre un chrono, stoppé automatiquement en fin de fonction

    // ... rendu ...
}
```

### GPU_STAGE_PROFILER

Encapsule une passe de rendu GPU avec les timestamps GL :

```c
void render_skybox(App* app) {
    GPU_STAGE_PROFILER(&app->gpu_profiler, "Skybox Pass", COLOR_CYAN);
    // ← Timestamps début/fin automatiques

    glDrawArrays(GL_TRIANGLES, 0, 3);
}
```

### RAII_SATISFY_*

Macros pour satisfaire l'analyse statique dans les patterns RAII :

```c
// Indique à clang-tidy que le pointeur est géré par RAII
Resource* res RAII_SATISFY_CLEANUP = resource_create();
```

Ces macros évitent les faux positifs de type « ressource non libérée » dans les analyseurs statiques.

## Exemple complet : Gestion de fichiers

```c
// Destructeur pour FILE*
static void cleanup_file(FILE** f) {
    if (*f) { fclose(*f); *f = NULL; }
}

// Usage
bool process_config(const char* path) {
    FILE* f __attribute__((cleanup(cleanup_file))) = fopen(path, "r");
    if (!f) return false;

    // ... traitement ...

    return true;
    // ← fclose() appelé automatiquement, même ici
}
```

## Cas d'utilisation dans le projet

| Macro | Type de ressource | Durée de vie |
|-------|------------------|-------------|
| `HYBRID_FUNC_TIMER` | Chronomètre CPU | Durée de la fonction |
| `GPU_STAGE_PROFILER` | Requête timer GL | Portée du bloc `{}` |
| `RAII_BUFFER_MAP` | Mapping de tampon GL | Portée du bloc `{}` |
| `RAII_DEBUG_GROUP` | `glPushDebugGroup` | Portée du bloc `{}` |

## Portabilité

L'attribut `cleanup` est supporté par :
- GCC 4.0+
- Clang 3.0+
- ARM Compiler 5.06+

Non supporté sur MSVC (compilation Windows via MinGW utilise GCC/Clang, donc compatible).

## Voir aussi

- [profiling_tracy.md](./profiling_tracy.md) — Usage de GPU_STAGE_PROFILER
- [gpu_profiling.md](./gpu_profiling.md) — Système de profilage GPU
- [synchronization_overview.md](./synchronization_overview.md) — Gestion des ressources de synchronisation
