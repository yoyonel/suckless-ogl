# Profilage avec Tracy

Ce document décrit l'utilisation du profileur Tracy pour l'analyse des performances en temps réel.

## Vue d'ensemble

[Tracy](https://github.com/wolfpld/tracy) est un profileur de frames en temps réel qui offre une visibilité simultanée sur les timings CPU et GPU avec une interface graphique interactive.

## Interface Tracy

### Timeline

L'interface principale affiche une timeline horizontale avec :
- **Lignes CPU** : Les zones HYBRID_MEASURE_LOG et les fonctions instrumentées
- **Lignes GPU** : Les passes de rendu GL avec timestamps hardware
- **Synchronisation** : Les points de synchronisation CPU/GPU

### Zones de mesure

Le projet utilise des macros adaptées selon le mode de build :

```c
// Mesure CPU+GPU simultanée (mode hybride)
HYBRID_MEASURE_LOG("Skybox Pass");
// ... commandes de rendu ...
// La zone se ferme automatiquement à la fin du scope (RAII)
```

En mode Release, les macros de profilage se compilent à zéro overhead grâce aux `#define` conditionnels.

## Analyse CPU/GPU

### Identifier les déséquilibres

Tracy révèle les cas où le CPU attend le GPU (ou vice versa) :

```
Frame 123:
  CPU: [Prepare---][Wait GPU...............][Present]
  GPU:             [Draw Pass][Post Process][Idle]
```

Ce pattern indique que le CPU est en attente du GPU — il faut soit optimiser les shaders, soit augmenter le parallélisme CPU/GPU.

### Zones HYBRID_MEASURE_LOG

Ces zones capturent à la fois le timing CPU (via `clock_gettime`) et le timing GPU (via `GL_TIMESTAMP`) pour la même passe :

```c
// Wrapper intégrant CPU + GPU
#define HYBRID_MEASURE_LOG(name) \
    CPUTracker _cpu_tracker_##__LINE__ = cpu_tracker_start(name); \
    GPUStageTracker _gpu_tracker_##__LINE__ = gpu_stage_start(name)
```

## Échantillonnage statistique

Pour identifier les fonctions CPU les plus coûteuses sans instrumentation manuelle :

1. Activer le mode échantillonnage Tracy (F2 dans l'interface)
2. Exécuter 30+ secondes de rendu
3. Consulter la vue "Perf Stats" pour le top des fonctions

## Configuration du build

```cmake
# Activer Tracy en Debug
cmake -B build -DENABLE_TRACY=ON

# Tracy est désactivé en Release pour un overhead nul
cmake -B build -DCMAKE_BUILD_TYPE=Release  # Tracy automatiquement désactivé
```

## Configuration du Port

Par défaut, Tracy utilise le port **8086** pour les données et le broadcast.
Si ce port est déjà utilisé sur votre système, le `Justfile` détecte
automatiquement un port libre :

```bash
# Automatique — utilise 8087 si 8086 est occupé
just tracy-server    # Terminal 1
just run-tracy       # Terminal 2

# Forçage manuel
TRACY_PORT=9090 just tracy-server
TRACY_PORT=9090 just run-tracy
```

La variable `tracy_port` du Justfile sonde le port 8086 avec `ss` et
bascule sur 8087 s'il est occupé. Le serveur utilise `-a 127.0.0.1 -p <port>`
pour une connexion directe (pas de broadcast UDP).

## Zones de Profilage N-Corps

Lorsque la simulation N-corps est active (`Shift+G`), les zones suivantes
apparaissent dans Tracy :

### Zones CPU (Thread Principal)

| Zone | Parent | Ce qu'elle mesure |
|------|--------|-------------------|
| `NBody Physics` | Frame | Wrapper top-level N-corps |
| `NBody Verlet` | NBody Physics | Gravité O(N²) + intégration Verlet |
| `NBody Trail Sample` | NBody Physics | Enregistrement ring buffer traînées |
| `NBody Instance Build` | NBody Physics | Génération matrices modèle |
| `NBody VBO Upload` | NBody Physics | Stall `glBufferSubData` |
| `Trail Ribbon Build` | NBody Trails | Staging géométrie CPU |
| `Trail VBO Upload` | NBody Trails | Upload buffer |
| `Trail Draw Calls` | NBody Trails | `glDrawArrays` par corps |

### Zones GPU (Contexte OpenGL)

| Zone | Ce qu'elle mesure |
|------|-------------------|
| `Instanced Render` | Draw call instancié des sphères |
| `NBody Trails` | Passe de rendu des traînées |

## Voir aussi

- [gpu_profiling.md](./gpu_profiling.md) — Système de profilage GPU intégré
- [profiling_guide.md](./profiling_guide.md) — Profilage ApiTrace
- [raii_cleanup_guide.md](./raii_cleanup_guide.md) — Macros RAII utilisées dans le profilage
