# Guide de profilage — ApiTrace

Ce document décrit le workflow complet d'analyse des performances avec ApiTrace.

## Vue d'ensemble

ApiTrace capture chaque appel OpenGL pendant l'exécution, permettant :
- Le replay frame par frame
- La mesure des temps d'exécution GPU par appel
- La détection des sous-optimisations pilote

## Installation

```bash
# Debian/Ubuntu
sudo apt install apitrace-gui

# Arch
sudo pacman -S apitrace

# Fedora
sudo dnf install apitrace
```

## Workflow de profilage

### 1. Capturer une trace

```bash
# Capture automatique (10 secondes)
just trace

# Capture manuelle
apitrace trace --api=gl -o trace.trace ./build/app
```

### 2. Analyser les métriques

```bash
# Analyse automatique avec le script intégré
just trace-perf

# Analyse manuelle
apitrace replay trace.trace
```

Le script `scripts/trace_analyze.py` extrait et formate les métriques clés :

```bash
# Sortie typique
Analyse de trace.trace
=======================
Total des images     :  1 247
Temps GPU total      : 18 234 ms
GPU moyen / image    :     14.6 ms (68.5 FPS équivalent)
Pire image           :     47.2 ms (image 523)

Top 5 des appels GPU les plus longs:
  1. glTexSubImage2D         : 1 847 ms total (14.7 ms/appel × 126 appels)
  2. glDrawArraysInstanced   : 4 231 ms total (0.43 ms/appel × 9 847 appels)
  3. glDispatchCompute       : 892 ms total (8.9 ms/appel × 100 appels)
```

### 3. Inspection visuelle

```bash
# Interface graphique frame par frame
qapitrace trace.trace
```

L'interface qapitrace permet de :
- Naviguer image par image
- Inspecter chaque état GL avant/après un appel
- Visualiser les textures et tampons

## Intégration en test unitaire automatico

Le projet inclut un test ApiTrace automatisé :

```bash
just test-apitrace           # Test léger (~5 secondes)
just test-integration-apitrace  # Test complet (~30 secondes)
```

Ces tests vérifient :
- Aucun appel `glGetError` retournant une erreur
- Temps GPU moyen < seuil cible (14 ms pour 60 FPS)
- Aucun avertissement de performance pilote

## Coder pour la profilabilité

### Étiquettes de débogage

Pour identifier clairement les blocs dans la trace :

```c
// Début d'une section profilée
glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Skybox Pass");

// ... commandes GL ...

glPopDebugGroup();
```

Ces étiquettes apparaissent dans qapitrace, facilitant la navigation.

### Appels à éviter dans le chemin critique

| API | Problème | Alternative |
|-----|---------|------------|
| `glGetError()` | Synchronisation implicite | Utiliser `GL_DEBUG_OUTPUT` en développement |
| `glFinish()` | Drain complet du pipeline | Clôtures `GLsync` |
| `glReadPixels()` synchrone | Blocage CPU+GPU | PBO double tampon |

## Voir aussi

- [profiling_tracy.md](./profiling_tracy.md) — Profilage temps réel Tracy
- [gpu_profiling.md](./gpu_profiling.md) — Système de profilage GPU intégré
- [debugging.md](./debugging.md) — Sortie debug GL
