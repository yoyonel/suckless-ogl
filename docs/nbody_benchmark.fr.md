# Benchmark d'Upload de Buffers NBody

## Objectif

Ce benchmark fournit une **mesure reproductible et automatisée** du coût
CPU des uploads de buffers par frame dans le pipeline de rendu NBody. Il capture
les chemins de code exacts identifiés comme sources de stalls de synchronisation
CPU↔GPU (voir [Optimisation des Uploads de Buffers NBody](nbody_buffer_optimization.fr.md)).

Le benchmark produit une **baseline** — une mesure de référence prise dans des
conditions contrôlées — qui remplit deux rôles :

1. **Valider les optimisations** : confirmer qu'un changement de code (ex :
   buffer orphaning) réduit effectivement la latence d'upload.
2. **Détecter les régressions** : si un futur refactoring ou ajout de
   fonctionnalité dégrade les performances d'upload, le benchmark le détectera.

## Ce Qui Est Mesuré

Le fichier de test `tests/test_benchmark_buffer_upload.c` contient trois tests :

### 1. `test_benchmark_nbody_instance_update`

Mesure le **pipeline de mise à jour des instances** — le chemin de code exécuté
à chaque frame pour envoyer les transformations et matériaux PBR des sphères
NBody vers le GPU :

```text
nbody_step()              → Intégration physique O(N²) (Velocity Verlet)
nbody_write_instances()   → Construction du tableau SphereInstance depuis l'état de la simulation
instanced_group_update()  → Orphan + upload du VBO d'instances (GL_DYNAMIC_DRAW)
glFinish()                → Forcer la complétion GPU pour un timing précis
```

La fenêtre de mesure couvre `nbody_write_instances` + `instanced_group_update`
+ `glFinish` — c'est-à-dire le coût **construction + upload + sync**. L'étape
de physique s'exécute en dehors de la mesure pour isoler la latence d'upload
du coût de calcul.

Le benchmark utilise un ordonnancement **draw → update** : le draw call du
frame précédent est soumis en premier, mettant le VBO "en vol" sur le GPU,
puis l'update est mesuré. Cela reproduit le scénario réel où l'orphaning
compte — le driver doit gérer un buffer encore en lecture par le GPU.

### 2. `test_benchmark_trail_renderer`

Mesure le **pipeline de rendu des traînées (ribbons)** — géométrie de rubans
orientés caméra, entièrement reconstruite sur le CPU à chaque frame :

```text
trail_renderer_record()   → Enregistrement des positions des corps dans les ring buffers
trail_renderer_draw()     → Construction des rubans + orphan + upload VBO + draw
glFinish()                → Forcer la complétion GPU pour un timing précis
```

La mesure couvre l'intégralité de l'appel `trail_renderer_draw` car la
construction des rubans (CPU), l'upload du VBO (CPU→GPU) et la soumission
du draw sont étroitement couplés dans cette fonction.

### 3. `test_instance_update_data_integrity`

Un **test de correction** (pas un benchmark). Vérifie que le buffer orphaning
ne corrompt pas les données en :

1. Exécutant un pas de physique via l'API réelle
2. Uploadant les instances via `instanced_group_update` (qui fait orphan + subdata)
3. Relisant le buffer GPU avec `glGetBufferSubData`
4. Comparant les valeurs metallic, roughness et albedo avec tolérance

Ce test existe car le buffer orphaning change le backing store — si le driver
ou l'application a un bug, les données pourraient être silencieusement perdues.

## Architecture : API Réelle, Pas de GL Synthétique

Le benchmark utilise **exclusivement les vraies fonctions de l'application** :

| Composant | Fonctions Utilisées |
|-----------|---------------------|
| Simulation NBody | `nbody_init_preset`, `nbody_step`, `nbody_get_count`, `nbody_write_instances` |
| Rendu instancié | `instanced_group_init`, `instanced_group_update`, `instanced_group_draw`, `instanced_group_bind_mesh` |
| Rendu des traînées | `trail_renderer_init`, `trail_renderer_record`, `trail_renderer_draw`, `trail_renderer_set_color` |
| Génération de mesh | `icosphere_generate`, `icosphere_free` |

Les seuls appels GL bruts sont :

- **Upload VBO/NBO/EBO de l'icosphère** : nécessaire car `instanced_group_bind_mesh`
  attend des buffer objects pré-existants (le mesh est de la géométrie statique,
  pas du chemin d'upload dynamique mesuré).
- **`glFinish()`** : force le drain du pipeline GPU pour obtenir un timing
  wall-clock précis.
- **`glGetBufferSubData()`** : dans le test d'intégrité, pour relire les données GPU.

Cette conception signifie que le benchmark **suit automatiquement les changements
de code**. Si `instanced_group_update` est refactoré (ex : passage aux persistent
mapped buffers), le benchmark mesure la nouvelle implémentation sans aucune
modification.

## Paramètres du Test

| Paramètre | Valeur | Justification |
|-----------|--------|---------------|
| `FRAMES` | 300 | Suffisant pour la stabilité statistique |
| `WARMUP_FRAMES` | 60 | Amorce le JIT du driver, remplit les ring buffers de traînées (256 slots) |
| `SIMULATED_DT` | 1/60s | Correspond au framerate réel de l'application |
| `BENCH_SUBDIVISIONS` | 3 | Correspond à `INITIAL_SUBDIVISIONS` (3840 indices) |
| Timer | `clock_gettime(CLOCK_MONOTONIC)` | Précision microseconde, monotone, pas de dérive NTP |

La phase de warmup est critique : sans elle, les ~30 premières frames montrent
des timings gonflés à cause de la compilation de shaders et de l'initialisation
du pool de buffers par le driver.

## Résultats de la Baseline

Capturés sur la branche `perf/nbody-buffer-orphaning` **avec orphaning appliqué** :

### Pipeline de Mise à Jour des Instances

```text
=== NBody Instance Update Benchmark ===
Bodies: 14  |  Mesh: 3840 indices (subdiv 3)
Frames: 300 (warmup: 60)
Avg update+upload: 734.6 µs  (4.41% of 60fps budget)
Min: 138.6 µs  |  Max: 7450.2 µs
Total update time: 220.4 ms over 300 frames
```

### Pipeline du Trail Renderer

```text
=== Trail Renderer Benchmark ===
Bodies: 14  |  Trail depth: 256 samples
Frames: 300 (warmup: 60)
Avg draw (build+upload+render): 830.2 µs  (4.98% of 60fps budget)
Min: 218.8 µs  |  Max: 5107.2 µs
Total draw time: 249.1 ms over 300 frames
```

### Interprétation des Chiffres

- **Moyenne (734/830 µs)** : le coût typique par frame. Combinés, les deux
  pipelines consomment ~1.56 ms soit **~9.4% du budget de 16.67 ms** à 60 FPS.
  Cela laisse ~90% pour tout le reste (PBR shading, post-processing, IBL, UI).

- **Min (138/218 µs)** : le meilleur cas — aucune contention, le driver recycle
  un buffer immédiatement. C'est le plancher théorique.

- **Max (7450/5107 µs)** : pics occasionnels dus à l'ordonnancement OS, au GC du
  driver, ou aux transitions de fréquence du GPU. Ces outliers sont normaux dans
  les benchmarks wall-clock et doivent être évalués par rapport à la moyenne,
  pas isolément.

- **% du budget 60fps** : la métrique clé. Si ce nombre approche 50%+, le
  pipeline d'upload est un goulot d'étranglement. Sous 10% signifie un overhead
  sain.

### Ce Que la Baseline Représente

Cette baseline CI a été capturée sur un **contexte Mesa/llvmpipe rendu logiciel**
(Xvfb, pas de GPU physique).

!!! warning "Limitation importante : pas de vrai pipeline GPU"
    Sur llvmpipe, il n'y a **pas de pipeline GPU asynchrone** — tout est
    exécuté de manière synchrone sur le CPU. Le stall CPU↔GPU que l'orphaning
    élimine **n'existe pas** dans ce contexte. Les timings mesurent
    essentiellement le coût de `memcpy` + overhead du driver logiciel,
    pas un vrai sync stall.

Concrètement :

1. Les timings absolus (µs) **ne sont pas représentatifs du matériel GPU réel**.
   Un GPU discret NVIDIA/AMD aura des caractéristiques radicalement différentes
   (DMA asynchrone, pool de buffers matériel, latence PCIe).
2. Les timings **relatifs** restent utiles pour détecter les **régressions de
   code** : si un refactoring ajoute accidentellement un O(N³) ou un memcpy
   supplémentaire, la baseline CI le détectera.
3. Cette baseline **ne mesure PAS l'efficacité de l'orphaning** — elle ne peut
   pas distinguer orphan vs non-orphan sur llvmpipe.

## Résultats A/B sur Matériel Réel

Mesurés avec `just bench-ab` sur **Intel Iris Xe (RPL-U), Mesa 25.0.7-2**,
3 runs consécutifs de 5 itérations chacun :

| Métrique | Run 1 | Run 2 | Run 3 | Moyenne |
|----------|-------|-------|-------|---------|
| `draw (build+upload+render)` | **-52.6%** | **-39.3%** | **-44.7%** | **~-45%** |
| `update+upload` | -8.7% | +2.6% | -9.1% | **~0%** (bruit) |

### Analyse

**Trail renderer (`draw`) : amélioration stable de ~45%.** Le pattern d'orphaning
élimine un vrai stall de synchronisation sur Mesa/Intel. Sans orphaning,
`glBufferSubData` doit attendre que le GPU finisse de lire l'ancien buffer avant
d'écrire le nouveau. Avec `glBufferData(NULL)` + `glBufferSubData`, Mesa alloue
un nouveau backing store immédiatement — pas de stall. Le stddev élevé côté
master (193–682 µs) confirme des stalls sporadiques qui disparaissent avec
l'orphaning.

**Mise à jour des instances (`update+upload`) : aucun gain mesurable.** Les
données d'instances sont petites (~50 Ko pour 14 corps × `sizeof(SphereInstance)`),
insuffisant pour provoquer un stall mesurable. La variation de ±5% est du bruit
statistique.

**Point clé :** le buffer orphaning est le plus efficace sur les buffers
volumineux et fréquemment reconstruits. La reconstruction de ~527 Ko par frame
du trail renderer est le principal bénéficiaire. Les données d'instances sont
trop petites pour causer un stall notable sur les drivers modernes.

!!! note "Comportement dépendant du driver"
    Certains drivers (ex : NVIDIA propriétaire) effectuent un orphaning
    implicite en interne, rendant le pattern explicite redondant. Mesa/Intel
    ne le fait pas, c'est pourquoi le gain est significatif sur ce matériel.

### Utilisation pour l'Optimisation (GPU Réel)

L'objectif premier de ce benchmark est de **guider le travail d'optimisation
du pipeline NBody**. Pour cela, il faut l'exécuter sur la machine de
développement avec le vrai GPU :

```bash
# Exécution directe avec GPU réel (pas de Xvfb)
./build/tests/test_benchmark_buffer_upload
```

Sur un GPU réel, le benchmark capture les vrais stalls de synchronisation :

- **Comparaison orphaning vs sans** : checkout master (sans orphaning),
  mesurer → checkout branche (avec orphaning), mesurer → comparer les `Avg`.
  Le delta représente le temps de stall éliminé.
- **Évaluation des futures optimisations** : double-buffering, persistent
  mapping, compute ribbons — chaque approche se mesure avec le même outil.
- **Profil du Max** : sur GPU réel, un `Max` élevé indique un vrai stall
  de synchronisation (le GPU n'avait pas fini de lire le buffer), pas juste
  du bruit OS.

| Contexte | Ce qui est mesuré | Utilité |
|----------|-------------------|----------|
| **Xvfb/llvmpipe** (CI) | memcpy + overhead driver logiciel | Détection de régression de code |
| **GPU réel** (dev) | Vrai stall CPU↔GPU + upload DMA | Validation d'optimisation, comparaison A/B |

## Comment Exécuter

### Exécution Rapide (GPU réel)

```bash
# Via recette Just — build si nécessaire, exécution directe (pas de Xvfb)
just bench-nbody

# Ou exécuter le binaire directement
./build/tests/test_benchmark_buffer_upload
```

### Exécution en CI / Suite de Tests (Xvfb)

```bash
# Sortie verbeuse via CTest (utilise le wrapper Xvfb)
cd build && ctest -R test_benchmark_buffer_upload -V

# Comme partie de la suite de tests complète
just test-all
```

## Comment Utiliser la Baseline

### Comparaison A/B Automatisée Entre Branches

La recette `just bench-ab` automatise le workflow complet de comparaison A/B :

```bash
# Comparer la branche courante vs master (5 runs par côté, défaut)
just bench-ab

# Comparer contre une branche spécifique avec un nombre de runs personnalisé
just bench-ab origin/main 10
```

La recette :

1. Vérifie que le working tree est propre (commit ou stash d'abord)
2. Build et exécute le benchmark N fois sur la branche courante
3. Bascule sur la branche de référence (cherry-pick automatique de tous les
   commits du test de benchmark s'il n'existe pas sur cette branche)
4. Build et exécute le benchmark N fois sur la référence
5. Retourne sur la branche originale
6. Affiche le renderer GL et la version utilisés pour la mesure
7. Calcule moyenne ± écart-type pour chaque métrique et affiche un tableau :

```text
Metric                            branche-courante         master       Delta    Change
──────────────────────────────  ───────────────  ───────────────  ──────────  ────────
update+upload                      500.1±45.1      517.5±19.9  -17.4 µs  ▼-3.4%
draw (build+upload+render)        1029.8±82.1      894.0±55.0  135.8 µs  ▲15.2%
```

- **▼ vert** = la branche courante est plus rapide (amélioration)
- **▲ rouge** = la branche courante est plus lente (régression)

!!! tip "GPU réel requis pour des résultats significatifs"
    Lancez `just bench-ab` depuis un terminal avec accès au vrai GPU.
    Ne **pas** exécuter sous Xvfb — les résultats ne mesureraient que
    l'overhead du rendu logiciel, pas les vrais stalls de synchronisation
    CPU↔GPU.

#### Fonctionnalités du Script A/B

- **Cache de build** : la branche de référence est compilée dans
  `build-bench-ref/` (séparé du `build/` courant). Ce répertoire persiste
  entre les runs, les comparaisons suivantes ne recompilent que les fichiers
  modifiés (incrémental).
- **Détection du renderer GL** : affiche le driver GPU et la version en haut
  des résultats. Avertit si un renderer logiciel (llvmpipe/softpipe) est
  détecté, ou si les deux côtés utilisent des renderers différents.
- **Cherry-pick multi-commit** : quand le test de benchmark n'existe pas sur
  la branche de référence, le script cherry-pick TOUS les commits touchant
  le fichier de test (pas seulement l'ajout initial), garantissant que des
  fonctionnalités comme l'affichage du renderer GL sont incluses.
- **Nettoyage automatique** : les cherry-picks temporaires ne sont jamais
  committés ; le script restaure la branche de référence dans son état
  original.

### Comparaison A/B Manuelle (alternative)

```bash
# 1. Exécuter le benchmark sur la branche courante, sauvegarder la sortie
just bench-nbody 2>&1 | tee /tmp/bench_current.txt

# 2. Basculer vers la branche de comparaison et rebuilder
git checkout master && just build

# 3. Re-exécuter le benchmark
just bench-nbody 2>&1 | tee /tmp/bench_master.txt

# 4. Comparer les lignes "Avg"
diff <(grep "Avg" /tmp/bench_master.txt) <(grep "Avg" /tmp/bench_current.txt)
```

### Garde de Régression CI

Le benchmark est enregistré dans `tests/CMakeLists.txt` comme partie de la
liste `OPENGL_TESTS` et s'exécute sous Xvfb en CI. Pour ajouter un seuil
de régression strict, enveloppez le test avec un timeout CTest ou ajoutez
des assertions :

```c
// Exemple : échouer si la moyenne dépasse 5ms (indiquerait un stall de sync)
TEST_ASSERT_MESSAGE(avg_us < 5000.0,
    "Instance update exceeds 5ms — possible sync regression");
```

Ceci n'est intentionnellement **pas encore activé** car le seuil absolu dépend
du matériel du runner CI. Une fois la stabilité de la baseline confirmée sur
plusieurs exécutions CI, un seuil pourra être calibré.

### Suivi Dans le Temps

Enregistrez les résultats du benchmark dans un simple log pour suivre les tendances :

```bash
echo "$(git rev-parse --short HEAD) $(date +%F) $(ctest -R test_benchmark_buffer_upload -V 2>&1 | grep 'Avg')" >> perf_log.txt
```

## Maintenance

### Quand Mettre à Jour le Benchmark

- **Nouveau chemin de buffer dynamique** : si un nouveau sous-système fait des
  uploads VBO par frame (ex : système de particules, simulation de tissu),
  ajoutez un test de benchmark suivant le même pattern.
- **Changement d'API** : si `instanced_group_update` ou `trail_renderer_draw`
  changent de signature, mettez à jour les appels du benchmark en conséquence
  (le compilateur attrapera la plupart des ruptures).
- **Changement de paramètre** : si `NBODY_MAX_BODIES` ou `TRAIL_MAX_POINTS`
  changent, le benchmark s'adapte automatiquement (il les lit depuis les headers).

### Ce Qu'il Ne Faut PAS Changer

- **Nombre de frames et warmup** : changer `FRAMES` ou `WARMUP_FRAMES` invalide
  la comparaison avec les baselines précédentes. Si changé, documentez la raison
  et ré-établissez la baseline.
- **Fonction timer** : `clock_gettime(CLOCK_MONOTONIC)` est l'horloge de
  référence. Ne pas passer à `glfwGetTime` (résolution inférieure) ou `rdtsc`
  (non portable).

## Relation avec les Autres Benchmarks

| Benchmark | Périmètre | Timer | Emplacement |
|-----------|-----------|-------|-------------|
| **Celui-ci (buffer upload)** | Latence CPU d'upload pour NBody | `clock_gettime` (wall-clock CPU) | `tests/test_benchmark_buffer_upload.c` |
| [Effect Benchmark](effect_benchmark.fr.md) | Coût GPU des effets de post-processing individuels | Timestamps GPU (`glQueryCounter`) | `src/effect_benchmark.c` |
| [Protocole de Benchmarking Perf](perf_benchmarking_protocol.fr.md) | Profiling GPU complet de l'application (multi-run, statistique) | Timestamps GPU + `AdaptiveSampler` | `scripts/perf_benchmark.py` |
| [Perf Billboard](billboard_optimization.fr.md) | Débit de rendu des billboards | Wall-clock CPU | `tests/test_billboard_perf.c` |

## Références

- [Optimisation des Uploads de Buffers NBody](nbody_buffer_optimization.fr.md) — l'optimisation que ce benchmark valide
- [OpenGL Wiki — Buffer Object Streaming](https://www.khronos.org/opengl/wiki/Buffer_Object_Streaming)
- [Protocole de Benchmarking de Performance](perf_benchmarking_protocol.fr.md) — méthodologie de benchmarking du projet
- [Effect Benchmark](effect_benchmark.fr.md) — mesure du coût GPU des effets
