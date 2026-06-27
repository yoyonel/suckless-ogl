# Débogage OpenGL

Ce document décrit les outils de débogage OpenGL intégrés dans le projet.

## Sortie de débogage OpenGL

Pour maximiser les performances sur les iGPU comme Intel Iris Xe (où les points de synchronisation CPU-GPU peuvent provoquer un bridage thermique/énergétique), le moteur utilise un modèle de débogage asynchrone hautement optimisé :

- **Contexte de débogage conditionnel :** Le contexte de débogage OpenGL (`GLFW_OPENGL_DEBUG_CONTEXT`) n'est demandé qu'en build Debug (`#ifndef NDEBUG`). En mode Release, les validations du pilote sont désactivées pour permettre des chemins d'exécution rapides.
- **Groupes de débogage sans surcoût :** Les annotations de débogage (`gl_debug_push_group` / `gl_debug_pop_group`) sont compilées sous forme de fonctions vides `static inline` en mode Release, éliminant complètement le surcoût d'appel CPU et de transition pilote.
- **Gestion asynchrone des erreurs :** Tous les appels d'exécution à la fonction synchrone `glGetError()` ont été éliminés pour éviter les blocages de pipeline CPU-GPU. Les erreurs sont à la place signalées asynchroniquement via `glDebugMessageCallback` en build Debug.

### Mode Haute Sensibilité (Builds Debug)

Lorsque le contexte de débogage est actif dans les builds Debug :

- La sortie synchrone (`GL_DEBUG_OUTPUT_SYNCHRONOUS`) est activée afin que le callback soit exécuté immédiatement lors de la génération d'un message par le pilote, permettant des traces de pile précises dans un débogueur.
- Un mécanisme de dédoublonnage évite de saturer les logs avec des messages identiques (par exemple, dans la boucle de rendu).
- Les messages de sévérité `GL_DEBUG_SEVERITY_NOTIFICATION` (ex: allocations de ressources verbeuses ou détails de mise en page) sont filtrés via `glDebugMessageControl` pour éviter le surcoût lié aux allers-retours du callback.

### Sévérité des messages

| Sévérité | Valeur GL | Journalisation | Signification |
|----------|-----------|---------------|---------------|
| `HIGH` | `GL_DEBUG_SEVERITY_HIGH` | `ERROR` | Erreurs critiques, comportement indéfini |
| `MEDIUM` | `GL_DEBUG_SEVERITY_MEDIUM` | `WARN` | Problèmes de performance majeurs, dépréciations |
| `LOW` | `GL_DEBUG_SEVERITY_LOW` | `WARN` | Avertissements de performance mineurs ou redondances |

### Messages de performance

Les messages de catégorie `GL_DEBUG_TYPE_PERFORMANCE` indiquent des sous-optimisations détectées par le pilote :

- `0x20072` — Mapping d'un tampon occupé (synchronisation implicite)
- `0x20084` — Utilisation d'un format de texture non natif
- `0x20092` — Écriture dans un tampon partagé

Ces messages sont traités comme des avertissements et doivent être résolus (voir [opengl_cleanup.md](./opengl_cleanup.md)).
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
