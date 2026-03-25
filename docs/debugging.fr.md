# Débogage OpenGL

Ce document décrit les outils de débogage OpenGL intégrés dans le projet.

## Sortie de débogage OpenGL

Le projet utilise le callback `GL_DEBUG_OUTPUT` pour capturer et journaliser tous les messages émis par le pilote OpenGL.

### Activation

```c
glEnable(GL_DEBUG_OUTPUT);
glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Mode haute sensibilité
glDebugMessageCallback(debug_callback, NULL);
```

Le mode `GL_DEBUG_OUTPUT_SYNCHRONOUS` garantit que le callback est invoqué sur le thread qui a émis la commande, ce qui permet d'obtenir une trace de pile précise lors du débogage.

### Sévérité des messages

| Sévérité | Valeur GL | Journalisation | Signification |
|----------|-----------|---------------|---------------|
| `HIGH` | `GL_DEBUG_SEVERITY_HIGH` | `ERROR` | Erreurs critiques, comportement indéfini |
| `MEDIUM` | `GL_DEBUG_SEVERITY_MEDIUM` | `WARN` | Problèmes de performance, dépréciations |
| `LOW` | `GL_DEBUG_SEVERITY_LOW` | `INFO` | Avertissements mineurs |
| `NOTIFICATION` | `GL_DEBUG_SEVERITY_NOTIFICATION` | `DEBUG` | Informations de diagnostic |

### Messages de performance

Les messages de catégorie `GL_DEBUG_TYPE_PERFORMANCE` indiquent des sous-optimisations détectées par le pilote :

- `0x20072` — Mapping d'un tampon occupé (synchronisation implicite)
- `0x20084` — Utilisation d'un format de texture non natif
- `0x20092` — Écriture dans un tampon partagé

Ces messages sont traités comme des avertissements et doivent être résolus (voir [opengl_cleanup.md](./opengl_cleanup.md)).

## Mode haute sensibilité

En build Debug, **tous** les messages GL sont activés, y compris les notifications. Cela permet de détecter les problèmes tôt, notamment :
- Les textures utilisées avant initialisation
- Les états GL incorrects
- Les erreurs de validation Vulkan (si applicable)

## ApiTrace

Pour capturer une trace complète des appels OpenGL :

```bash
# Capturer une trace
just trace

# Analyser la trace
just trace-perf

# Replay graphique
qapitrace trace.trace
```

ApiTrace permet d'inspecter chaque appel GL frame par frame et de mesurer les temps d'exécution GPU.

## Voir aussi

- [opengl_cleanup.md](./opengl_cleanup.md) — Résolution des avertissements GL courants
- [profiling_guide.md](./profiling_guide.md) — Workflow ApiTrace complet
- [renderdoc_guide.md](./renderdoc_guide.md) — Capture et analyse GPU avancée
