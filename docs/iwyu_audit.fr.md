# Audit Include-What-You-Use (IWYU)

## Présentation

[Include-What-You-Use](https://include-what-you-use.org/) (IWYU) est un outil
d'analyse statique qui vérifie que chaque fichier source C/C++ inclut
exactement ce qu'il utilise — ni plus, ni moins. Il réduit les temps de
compilation, prévient le couplage transitif des dépendances, et maintient le
graphe d'inclusions propre.

Ce projet utilise IWYU dans sa stratégie d'hygiène des dépendances (voir
[Architecture](architecture.fr.md) pour le graphe de dépendances d'inclusions).

## Installation

```bash
# Debian/Ubuntu
sudo apt install -y iwyu

# Ou via nala
sudo nala install -y iwyu

# Vérification
iwyu --version
# Attendu : include-what-you-use 0.23 based on clang ...
```

IWYU nécessite une **base de données de compilation compatible Clang**
(`compile_commands.json`). Le projet en génère une automatiquement via CMake
dans le répertoire `build/`.

## Exécution de l'audit

### Scan complet du projet

```bash
# Lancer IWYU sur toutes les unités de traduction
iwyu_tool -p build 2>&1 > /tmp/iwyu_full.txt

# Sans suggestions de forward-declarations (recommandé pour les projets C)
iwyu_tool -p build -- -Xiwyu --no_fwd_decls 2>&1 > /tmp/iwyu_full.txt
```

### Fichier unique

```bash
# Analyser un seul fichier avec ses flags de compilation
iwyu_tool -p build -- -Xiwyu --no_fwd_decls src/app.c 2>&1
```

### Filtrage des résultats

```bash
# Afficher uniquement les fichiers avec des suggestions
grep -E "should (add|remove)" /tmp/iwyu_full.txt

# Compter le total des includes supprimables
grep "^- #include" /tmp/iwyu_full.txt | wc -l

# Extraire le résumé par fichier (headers projet uniquement)
grep -A100 "should remove" /tmp/iwyu_full.txt \
  | grep '^- #include "' \
  | sort | uniq -c | sort -rn
```

## Interprétation des résultats

La sortie d'IWYU suit ce format pour chaque unité de traduction :

```text
path/to/file.c should add these lines:
#include <stdint.h>      // uint32_t
#include "other.h"       // SomeType

path/to/file.c should remove these lines:
- #include <stdio.h>     // lines 5-5
- #include "unused.h"    // lines 8-8

The full include list for path/to/file.c:
#include "file.h"
#include <stdint.h>
#include "other.h"
```

### Sections

| Section | Signification |
|---------|---------------|
| **should add** | Headers utilisés directement mais non inclus |
| **should remove** | Headers inclus mais non utilisés directement |
| **full include list** | Ce qu'IWYU considère être l'ensemble correct d'includes |
| **has correct #includes** | Le fichier n'a besoin d'aucune modification |

### Faux positifs courants

IWYU est puissant mais imparfait, surtout pour les projets C avec macros et
compilation conditionnelle. **Toujours vérifier avant d'appliquer** :

| Pattern | Pourquoi c'est un faux positif |
|---------|-------------------------------|
| `gl_common.h` → `glad/glad.h` | `gl_common.h` fournit des macros RAII (`GL_SAFE_DELETE_*`, `GL_SCOPE_*`) au-delà des types GL. Le remplacer casse le contrat d'API. |
| `immintrin.h` → `emmintrin.h` + `smmintrin.h` | `immintrin.h` est le header parapluie pour SSE/AVX/F16C. Le découper fait perdre les intrinsics AVX-256. |
| `sched.h` supprimé d'un header | Le header définit un `struct sched_param` dans un champ de struct — IWYU rate parfois les dépendances de champs. |
| Headers système supprimés d'un `.h` | Le `.c` utilise les types (`va_list`, `uintptr_t`) transitifs. Déplacer l'include vers le `.c` est correct ; le supprimer ne l'est pas. |
| `app_settings.h` supprimé | Peut causer des échecs en cascade — les fichiers en aval dépendent des constantes transitives (`DEFAULT_EXPOSURE_STEP`). Correctif : ajouter les includes directs dans les consommateurs. |

### Workflow recommandé

1. **Lancer IWYU** et sauvegarder la sortie complète
2. **Catégoriser** les suppressions :
   - **Headers système** (`stdio.h`, `string.h`, etc.) — généralement sûr, mais tester chacun
   - **Headers projet** (`app_settings.h`, `shader.h`) — vérifier l'usage des symboles d'abord
   - **Remplacements `gl_common.h`** — reporter au refactoring header-fanout dédié
3. **Appliquer** les suppressions par batch, builder après chaque batch
4. **Corriger les cascades** : quand on supprime un include transitif d'un header, les consommateurs peuvent avoir besoin d'un include direct
5. **Lancer la suite de tests** séquentiellement (`ctest --test-dir build --output-on-failure`)
6. **Vérifier** que format + lint sont propres

## Résultats de l'audit du projet (Avril 2026)

L'audit IWYU initial de suckless-ogl a identifié :

| Catégorie | Nombre | Statut |
|-----------|-------:|--------|
| Suppressions headers système | 26 | Appliquées (3 étaient des faux positifs) |
| Suppressions headers projet | 8 | Appliquées |
| `gl_common.h` → `glad/glad.h` | 33 | Reporté (nécessite refactoring header-fanout) |
| Faux positifs détectés | 4 | `sched.h`, `immintrin.h`, `stdarg.h`+`stdint.h` dans `utils.h` |
| Corrections en cascade | 2 | `src/utils.c` (+`stdarg.h`/`stdint.h`), `src/postprocess_input.c` (+`app_settings.h`) |
| **Includes supprimés (net)** | **35** | Sur 29 fichiers |

### Taux de faux positifs

Sur 41 suppressions tentées, 4 étaient des faux positifs (**~10%**). Cela
confirme que les suggestions IWYU doivent toujours être traitées comme
consultatives, pas autoritatives.

## Intégration CI

IWYU n'est pas (encore) appliqué en CI. La stratégie actuelle est un audit
manuel périodique. Voir l'issue [#266](https://github.com/yoyonel/suckless-ogl/issues/266)
pour la gate CI de métriques de dépendances prévue.

## Voir aussi

- [Architecture — Graphe de dépendances d'inclusions](architecture.fr.md)
- [Stratégie de linting](linting_strategy.md)
- [Issue #261 — Audit IWYU](https://github.com/yoyonel/suckless-ogl/issues/261)
- [Issue #266 — Gate CI de métriques de dépendances](https://github.com/yoyonel/suckless-ogl/issues/266)
