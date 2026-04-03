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

### Hypothèse Révisée — Bulle de Pipeline CPU-GPU (Confiance : 95%)

L'analyse timeline Tracy a révélé la **vraie cause racine** :

```text
GPU: [==Scene+PP+UI==][.........Swap Buffers (IDLE).........][==frame suivant==]
CPU: [==commandes GL==][Tracy][Swap][Collect][Poll][Update][==commandes GL==]
                        <---- GPU idle pendant travail CPU non-GL ---->
```

**Preuves Tracy (Frame 3,392) :**

- Exécution CPU Total Frame : **3.21 ms** (self time : 52 µs = 1.63%)
- Travail GPU utile : ~2.0–2.5 ms par frame
- Gap GPU idle (visible comme "Swap Buffers" dans la lane GPU) : ~1.0–1.5 ms
- Temps GPU idle / frame total ≈ **30-40%** → correspond à MangoHud ~63%

**Le problème est structurel** : la boucle principale exécute du travail CPU-only (physique, caméra, matrices, Tracy, polling) **après** la soumission de toutes les commandes GL et **avant** la soumission du frame suivant. Le GPU finit son travail et est en famine.

### Table de Confiance Mise à Jour

| # | Constat | Impact | Confiance | Méthode |
|:---:|:---|:---:|:---:|:---|
| ~~H1~~ | Stall readback queries | ~37 µs (négligeable) | **Mesuré** | Tracy Statistics |
| ~~H2~~ | Flush barrières tri | ~55 µs (négligeable) | **Mesuré** | Tracy Statistics |
| ~~H3~~ | Sync implicite UBO | < 10 µs (négligeable) | **Mesuré** | Tracy Statistics |
| H4 | Bande passante mémoire partagée | Structurel, pas primaire | 40% | Inchangé |
| **H5** | **Bulle de pipeline CPU-GPU (ordre boucle principale)** | **~30-40% GPU idle** | **95%** | **Tracy Timeline** |

## Correction Proposée — Réordonnancement de la Boucle Principale

Actuellement (`app_run()` dans `app.c`) :

```text
PollEvents → physique/caméra → App Update → Render (cmds GL) → Tracy → SwapBuffers → Collect
```

Proposé :

```text
PollEvents → Render (cmds GL) → SwapBuffers → physique/caméra/App Update → Collect
```

En déplaçant le travail CPU-only **après** SwapBuffers, le CPU prépare le frame N+1 **pendant** que le GPU exécute le frame N. La bulle de pipeline disparaît.

**Risques :**

- Les données caméra/physique seront **décalées d'un frame** par rapport au rendu (ajoute 1 frame de latence d'input). À 154 FPS c'est ~6.5 ms — acceptable pour ce cas d'usage.
- Certaines mises à jour (resize, chargement async) nécessiteront un ordonnancement soigneux.

## Phase 1 : Instrumentation Tracy (Fait)

Ajout de marqueurs CPU `PROFILE_ZONE` aux points de synchronisation clés :

| Zone | Fichier | Objectif |
|:---|:---|:---|
| `"GPU Query Readback (sync)"` | `gpu_profiler.c` | Mesurer la boucle bloquante `glGetQueryObjectui64v` |
| `"GI Probe Sync (buffer upload)"` | `scene.c` | `glBufferSubData` SSBO + packing texture 3D pour les sondes GI |
| `"GPU Sort: SSBO Upload"` | `sphere_sorting.c` | Transfert des données d'instances vers le GPU |
| `"GPU Sort: Compute Dispatch"` | `sphere_sorting.c` | Chaîne complète dispatch + barrières |
| `"PostProcess UBO Upload"` | `postprocess.c` | Détection de sync implicite `glBufferSubData` |

## Phase 2 : Réordonnancement de la Boucle Principale (Planifié)

Réorganiser `app_run()` pour chevaucher le travail CPU avec l'exécution GPU.
