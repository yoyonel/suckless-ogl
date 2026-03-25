# Mode Performance et notifications

Ce document décrit le système de mode performance et le mécanisme de notifications d'actions.

## Mode Performance (PerfMode)

Le mode performance active des optimisations système pour améliorer la fréquence d'images lors des benchmarks et des sessions exigeantes.

### Architecture basée sur le contexte

Le `PerfMode` utilise une approche contextuelle qui s'adapte automatiquement à l'environnement d'exécution :

```c
typedef enum {
    PERF_BACKEND_NONE       = 0, // Aucune optimisation
    PERF_BACKEND_GAMEMODE   = 1, // GameMode (Feral Interactive)
    PERF_BACKEND_NATIVE     = 2, // cpupower / syscall direct
} PerfBackend;
```

### GameMode

Si [GameMode](https://github.com/FeralInteractive/gamemode) est installé et actif, le system l'utilise en priorité :

```c
// Activer GameMode
int result = gamemode_request_start();
if (result == 0) {
    LOG_INFO("GameMode activé");
    backend = PERF_BACKEND_GAMEMODE;
}
```

GameMode configure automatiquement :
- Gouverneur CPU en mode `performance`
- Priorité processus élevée
- Désactivation des économies d'énergie GPU

### Fallback natif

Si GameMode n'est pas disponible, le fallback natif utilise `cpupower` pour configurer directement le gouverneur CPU :

```bash
# Activé via le module natif
cpupower frequency-set -g performance
```

## ActionNotifier — Tampon circulaire de notifications

Le système de notifications de l'application utilise un **tampon circulaire** pour afficher les actions récentes sans interrompre le flux de rendu.

### Architecture

```c
#define NOTIFIER_BUFFER_SIZE 8

typedef struct {
    char messages[NOTIFIER_BUFFER_SIZE][NOTIFIER_MSG_MAX];
    float timestamps[NOTIFIER_BUFFER_SIZE];
    int head;
    int count;
} ActionNotifier;
```

### Avantages du tampon circulaire

- **Pas d'allocation dynamique** : Taille fixe, connue à la compilation
- **Thread-safe** : Accès atomique via `head` et `count`
- **Pas de perte de message** : Les anciens messages sont effacés automatiquement lorsque le tampon est plein
- **Coût O(1)** : Insertion et suppression en temps constant

### Rendu des notifications

Les notifications sont affichées dans le coin supérieur droit de l'écran avec une atténuation progressive :

```glsl
// Opacité décroissante basée sur l'âge du message
float age = current_time - timestamp;
float alpha = max(0.0, 1.0 - age / NOTIFICATION_DURATION);
```

## Voir aussi

- [keyboard_system.md](./keyboard_system.md) — Raccourcis qui génèrent des notifications
- [ui_visual_parameters.md](./ui_visual_parameters.md) — Paramètres visuels de l'interface
