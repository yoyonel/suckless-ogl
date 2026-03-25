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

## Voir aussi

- [gpu_profiling.md](./gpu_profiling.md) — Système de profilage GPU intégré
- [profiling_guide.md](./profiling_guide.md) — Profilage ApiTrace
- [raii_cleanup_guide.md](./raii_cleanup_guide.md) — Macros RAII utilisées dans le profilage
