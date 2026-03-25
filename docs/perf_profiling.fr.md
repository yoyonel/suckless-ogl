# Profilage avec perf

Ce document décrit l'utilisation de `perf` pour le profilage CPU de bas niveau.

## Prérequis

### Permissions perf

```bash
# Vérifier la valeur actuelle (doit être ≤ 1 pour le profilage sans root)
cat /proc/sys/kernel/perf_event_paranoid

# Autoriser le profilage utilisateur
echo 1 | sudo tee /proc/sys/kernel/perf_event_paranoid
```

## Utilisation de base

```bash
# Enregistrer un profil d'exécution
perf record -g ./build/app

# Afficher le rapport
perf report --stdio

# Via Just
just perf
```

## Analyse avec perf stat

```bash
# Statistiques de compteurs matériels
perf stat -e cache-misses,cache-references,cycles,instructions ./build/app

# Résultat typique
# 1,234,567  cache-misses  (15.2% miss rate)
# 8,123,456  cache-references
```

## Intégration ApiTrace

Pour combiner le profilage CPU avec l'analyse des appels OpenGL :

```bash
# Capturer une trace + profil CPU simultanément
perf record -g -- apitrace trace --api=gl ./build/app

# Analyser la trace OpenGL
apitrace replay trace.trace
qapitrace trace.trace  # Interface graphique
```

## Autres outils disponibles

| Outil | Usage | Commande |
|-------|-------|---------|
| **ApiTrace** | Replay GL frame par frame | `just trace-perf` |
| **Valgrind** | Fuites mémoire et accès invalides | `just memcheck` |
| **ASan** | AddressSanitizer (runtime) | `just asan` |
| **FlameGraph** | Visualisation des traces de pile | [cpu_profiling_flamegraph.md](./cpu_profiling_flamegraph.md) |

## Voir aussi

- [cpu_profiling_flamegraph.md](./cpu_profiling_flamegraph.md) — FlameGraphs à partir des données perf
- [profiling_guide.md](./profiling_guide.md) — Guide ApiTrace complet
- [profiling_tracy.md](./profiling_tracy.md) — Profilage temps réel avec Tracy
