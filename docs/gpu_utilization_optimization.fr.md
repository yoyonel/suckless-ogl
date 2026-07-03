# Optimisation de l'Utilisation GPU

Objectif : maximiser l'utilisation GPU vers 100% sur l'environnement de développement principal (Intel Iris Xe, i7-1355U).

## Mesures de Référence (2026-04-03)

| Métrique | Intel Iris Xe (iGPU) | NVIDIA 950M (dGPU) |
|:---|:---:|:---:|
| **Utilisation GPU (MangoHud)** | ~63% | ~99% |
| **FPS** | 154 | 129 |
| **Total Frame** | 6.85 ms | 7.75 ms |
| **Scene Render** | 2.25 ms | 3.29 ms |
| **Billboard Render** | 1.31 ms | 2.55 ms |
| **Post-Process** | 3.20 ms | 3.39 ms |
| **Swap Buffers** | 1.16 ms | 0.87 ms |

## Évolution de l'Analyse

### Hypothèses Initiales (Avant Tracy)

| # | Hypothèse | Impact Estimé | Confiance |
|:---:|:---|:---:|:---:|
| H1 | Le readback des queries GPU (`glGetQueryObjectui64v`) bloque le CPU | 15-25% GPU idle | 70% |
| H2 | Les barrières du tri bitonique GPU flushent le pipeline | 2-3% | 50% |
| H3 | `glBufferSubData` du UBO PostProcess cause un sync implicite sur Mesa | 3-5% | 40% |
| H4 | La bande passante mémoire partagée (iGPU) limite le débit | structurel | 60% |

### Résultats de l'Instrumentation Tracy (Mesuré)

Ajout de marqueurs `PROFILE_ZONE` autour des points de synchronisation clés. Les statistiques Tracy révèlent :

| Zone | Moyenne | Médiane | P99 | Verdict |
|:---|:---:|:---:|:---:|:---|
| GPU Query Readback (sync) | 37 µs | 33 µs | 94 µs | **Pas un goulot** — négligeable |
| GPU Sort: SSBO Upload | 28 µs | — | — | **Pas un goulot** |
| GPU Sort: Compute Dispatch | 55 µs | 50 µs | 135 µs | **Pas un goulot** |
| PostProcess UBO Upload | (< 10 µs) | — | — | **Pas un goulot** |

**Les quatre hypothèses initiales ont été invalidées par la mesure.** Aucun de ces points de synchronisation ne cause de stall significatif.

### Hypothèse Révisée — Bulle de Pipeline CPU-GPU (Confiance : ~~95%~~ → **Invalidée**)

L'analyse initiale de la timeline Tracy suggérait une bulle de pipeline CPU-GPU :

```text
GPU: [==Scene+PP+UI==][.........Swap Buffers (IDLE).........][==frame suivant==]
CPU: [==commandes GL==][Tracy][Swap][Collect][Poll][Update][==commandes GL==]
                        <---- GPU idle pendant travail CPU non-GL ---->
```

**Cette hypothèse a été testée et invalidée.** Le réordonnancement de la boucle principale (déplacement du travail CPU-only après SwapBuffers) n'a produit aucune amélioration mesurable — le gap d'idle GPU est resté identique. L'idle GPU "Swap Buffers" n'était pas causé par une bulle de pipeline mais par un throttling externe (voir H6).

### Cause Racine Finale — Overhead du Compositeur X11/Compiz (Confiance : 95%)

Un profilage Tracy plus approfondi en plein écran a révélé la **vraie cause racine** :

**Preuves Tracy (Frame 2,053, plein écran, VSync OFF) :**

| Zone | Temps | % du Frame |
|:---|:---:|:---:|
| `GLFW PollEvents` | **625 µs** | **28%** |
| Travail GPU utile | ~200 µs | 9% |
| Soumission rendu + Swap | ~400 µs | 18% |
| GPU idle (famine) | ~1000 µs | **45%** |

`glfwPollEvents()` appelle le serveur X11, qui est médié par le gestionnaire de fenêtres compositing Compiz. Chaque aller-retour entraîne une latence significative comparée au modèle direct de Wayland.

**Comparaison multi-plateforme :**

| Environnement | Display | Compositeur | `PollEvents` | Usage GPU |
|:---|:---|:---|:---:|:---:|
| Intel Iris Xe (i7-1355U) | X11 | Compiz | ~625 µs | ~63% |
| NVIDIA 950M (Bazzite) | Wayland | Natif | ~10-50 µs | ~99% |

Le GPU est idle ~37% du temps car le CPU passe 625 µs par frame dans le polling d'événements X11/Compiz — temps pendant lequel aucune commande GL n'est soumise. Le NVIDIA 950M atteint 100% non pas parce qu'il est plus lent, mais parce que le modèle d'événements Wayland a un overhead négligeable.

**Il s'agit d'une limitation système, pas d'un bug applicatif.** Aucun changement de code ne peut réduire la latence de `glfwPollEvents()` sous X11/Compiz.

### Table de Confiance Mise à Jour

| # | Constat | Impact | Confiance | Méthode |
|:---:|:---|:---:|:---:|:---|
| ~~H1~~ | Stall readback queries | ~37 µs (négligeable) | **Mesuré** | Tracy Statistics |
| ~~H2~~ | Flush barrières tri | ~55 µs (négligeable) | **Mesuré** | Tracy Statistics |
| ~~H3~~ | Sync implicite UBO | < 10 µs (négligeable) | **Mesuré** | Tracy Statistics |
| H4 | Bande passante mémoire partagée | Structurel, pas primaire | 40% | Inchangé |
| ~~H5~~ | ~~Bulle de pipeline CPU-GPU~~ | ~~30-40% GPU idle~~ | **Invalidée** | Réordonnancement testé, aucun effet |
| **H6** | **Overhead polling événements X11/Compiz** | **~28% temps frame, ~37% GPU idle** | **95%** | **Tracy Timeline (plein écran)** |

## Correction Proposée — Réordonnancement de la Boucle Principale (**Testé — Aucun Effet**)

L'approche de réordonnancement a été implémentée et testée :

```text
Avant : PollEvents → physique/caméra → App Update → Render → Tracy → SwapBuffers → Collect
Après : PollEvents → Render → SwapBuffers → physique/caméra/App Update → Collect
```

**Résultat :** Aucun changement mesurable d'utilisation GPU ou de temps de frame. Le réordonnancement a été reverté car :

1. Il ajoutait 1 frame de latence d'input pour zéro bénéfice
2. Le gap d'idle GPU était causé par X11/Compiz, pas par l'ordonnancement du travail CPU

## Conclusion

L'utilisation GPU de ~63% sur Intel Iris Xe sous X11/Compiz est une **caractéristique système**, pas un défaut applicatif. Le GPU rend la scène en ~200 µs mais le CPU passe ~625 µs dans le polling d'événements X11 par frame, privant le GPU de nouveau travail.

**Options d'atténuation (toutes externes à l'application) :**

| Option | Impact Attendu | Faisabilité |
|:---|:---|:---|
| Passer à un compositeur Wayland | GPU → ~100% | Nécessite un changement d'environnement de bureau |
| Utiliser un WM non-compositing (ex. i3, dwm) | Overhead PollEvents réduit | Préférence utilisateur |
| Désactiver la composition Compiz (`compiz --replace --no-composite`) | Amélioration partielle | Peut casser des fonctionnalités bureau |

Aucune modification de code côté application n'est prévue pour ce problème.

## Phase 1 : Instrumentation Tracy (Fait)

Ajout de marqueurs CPU `PROFILE_ZONE` aux points de synchronisation clés :

| Zone | Fichier | Objectif |
|:---|:---|:---|
| `"GPU Query Readback (sync)"` | `gpu_profiler.c` | Mesurer la boucle bloquante `glGetQueryObjectui64v` |
| `"GI Probe Sync (buffer upload)"` | `scene.c` | `glBufferSubData` SSBO + packing texture 3D pour les sondes GI |
| `"GPU Sort: SSBO Upload"` | `billboard_sorter.c` | Transfert des données d'instances vers le GPU |
| `"GPU Sort: Compute Dispatch"` | `billboard_sorter.c` | Chaîne complète dispatch + barrières |
| `"PostProcess UBO Upload"` | `postprocess.c` | Détection de sync implicite `glBufferSubData` |

## Phase 2 : Réordonnancement de la Boucle Principale (**Testé — Invalidé**)

Réorganisation de `app_run()` pour déplacer le travail CPU-only (physique caméra, mise à jour UI, notifier, sampler) après `glfwSwapBuffers()`. Le changement compilait et passait les 60/60 tests mais n'a produit **aucune amélioration** de l'utilisation GPU. Le réordonnancement a été reverté.

Cela a mené à la découverte de la vraie cause racine (H6 : overhead X11/Compiz) via une instrumentation Tracy supplémentaire de la boucle frame complète.

### Zones Tracy Supplémentaires (Diagnostic Phase 2)

| Zone | Fichier | Objectif |
|:---|:---|:---|
| `"Frame Timing"` | `app.c` | Bloc timing/FPS/sampler |
| `"UI & Notifier Update"` | `app.c` | Notifier d'actions, overlay UI, temps postprocess |
| `"Camera Physics"` | `app.c` | Physique timestep fixe + interpolation rotation |
| `"PostProcess Resize"` | `app.c` | Recréation différée FBO/textures |
| `"Icosphere Regen"` | `app.c` | Régénération mesh sur changement de subdivision |
