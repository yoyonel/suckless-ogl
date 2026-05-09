# Stratégie de lint et mise en cache

Ce document présente la stratégie d'analyse statique et de formatage du projet `suckless-ogl` et l'implémentation de son mécanisme de mise en cache haute performance.

## Philosophie du style de code

Ce projet suit un style de code fortement inspiré du **noyau Linux** ([`Documentation/process/coding-style.rst`](https://www.kernel.org/doc/html/latest/process/coding-style.html)), adapté pour un moteur de rendu OpenGL en C11.

### Pourquoi le style du noyau Linux ?

Le style du noyau a été conçu pour des bases de code C à grande échelle, maintenues par des milliers de contributeurs pendant des décennies. Ses règles optimisent la **lisibilité à l'échelle**, la **facilité de recherche** (grep-ability) et la **charge cognitive minimale** — les mêmes qualités que nous valorisons dans ce projet.

Autres conventions C notables dont nous nous sommes inspirés :

| Convention | Origine | Point clé |
|---|---|---|
| [**Linux kernel coding style**](https://www.kernel.org/doc/html/latest/process/coding-style.html) | Torvalds, communauté kernel | Inspiration principale : tabs-8, accolades, 80 cols, `goto` cleanup, fonctions courtes |
| [**SEI CERT C**](https://wiki.sei.cmu.edu/confluence/display/c) | Carnegie Mellon SEI | Règles orientées sécurité (MEM, ERR, STR) — appliquées via les checks `clang-tidy cert-*` |
| [**FreeBSD style(9)**](https://man.freebsd.org/cgi/man.cgi?query=style&sektion=9) | Projet FreeBSD | Proche du style kernel ; explicite sur les lignes vides entre fonctions, `return` sans parenthèses |
| [**SQLite**](https://www.sqlite.org/codeofethics.html) | D. Richard Hipp | 100% couverture de branches, `goto` cleanup, C autonome — un modèle de qualité |
| [**id Software**](https://github.com/id-Software/Quake-III-Arena) (Quake/Doom) | John Carmack | C pragmatique pour le graphisme temps réel ; nommage et disposition orientés performance |

### Principes fondamentaux (issus du kernel)

1. **Indentation = 8 espaces (tabulations)** — Force un nesting court ; si 3+ niveaux sont nécessaires, refactorer (kernel §1)
2. **Limite de 80 colonnes** — Encourage la séparation des expressions complexes et l'extraction de fonctions helpers (kernel §2)
3. **Accolade ouvrante sur la même ligne** (sauf fonctions) — Style Linux/K&R (kernel §3)
4. **Une ligne vide entre les définitions de fonctions** — Séparation visuelle ; ne jamais empiler des définitions sans respiration
5. **Les fonctions doivent être courtes** — Faire une seule chose ; tenir sur un ou deux écrans (kernel §6)
6. **`goto` pour le cleanup centralisé** — Chemin de sortie unique pour la gestion d'erreurs, pas de `if/else` en cascade (kernel §7). Voir [Pattern goto cleanup](goto_cleanup_pattern.fr.md)
7. **`snake_case` partout** — Fonctions, variables, membres de struct. `UPPER_CASE` uniquement pour les macros (kernel §4)
8. **Pas de typedef sur les structs** — Exception : handles opaques et pointeurs de fonctions (kernel §5)

## Formatage du code (Clang-Format)

Tous les fichiers C, en-têtes et GLSL sont formatés avec `clang-format`. La configuration se trouve dans `.clang-format` à la racine du dépôt.

### Référence des règles

Chaque règle est associée à sa justification dans le style kernel :

| Option | Valeur | Référence kernel | Effet |
|--------|--------|-----------------|-------|
| `BasedOnStyle` | `Google` | — | Style de base (surchargé ci-dessous pour correspondre aux conventions kernel) |
| `IndentWidth` | `8` | §1 : « Tabs are 8 characters » | Indentation logique de 8 espaces ; décourage le nesting profond |
| `UseTab` | `ForIndentation` | §1 : « use tabs » | Tabulations pour l'indentation, espaces pour l'alignement |
| `BreakBeforeBraces` | `Linux` | §3 : « K&R brace placement » | Accolade ouvrante sur la même ligne ; fonctions sur la ligne suivante |
| `PointerAlignment` | `Left` | §4 : « declare close to the type » | Style `int* p` (convention C) |
| `ColumnLimit` | `80` | §2 : « the preferred limit is 80 columns » | Retour à la ligne strict à 80 colonnes |
| `AllowShortFunctionsOnASingleLine` | `None` | §6 : les fonctions doivent être lisibles | Ne jamais compacter les corps de fonctions sur une seule ligne |
| `SeparateDefinitionBlocks` | `Always` | §1, FreeBSD style(9) | **Impose une ligne vide entre chaque définition de top-level** (fonctions, structs, enums) |
| `SortIncludes` | `true` | — | Tri alphabétique des includes (GLAD en priorité via `IncludeCategories`) |

### `SeparateDefinitionBlocks`

Cette règle (disponible depuis clang-format 14) garantit une séparation visuelle cohérente entre les définitions de fonctions, les déclarations de structs et les définitions d'enums. Sans elle, `clang-format` ne touche pas à l'espacement entre les définitions — des corps de fonctions adjacents sans ligne vide passent silencieusement.

Avec `Always`, `clang-format` insère automatiquement une ligne vide là où il en manque une, et supprime les doubles lignes vides superflues.

Le noyau Linux applique cette règle par convention lors des revues de code. Le `style(9)` de FreeBSD l'énonce explicitement : *« Use blank lines to separate logical sections of code, including between function definitions. »* Nous l'automatisons via clang-format.

### Application en CI

Le formatage est appliqué en CI par le job `lint-and-format` :

```yaml
# .github/workflows/main.yml (extrait)
make format CONTAINER_RUN=""
git diff --exit-code || (echo "❌ Format error" && exit 1)
```

Tout fichier qui serait reformaté par `clang-format` fait échouer le job. Exécutez `just format` localement avant de commiter.

## Stratégie : Clang-Tidy

Nous utilisons `clang-tidy` pour l'analyse statique. La configuration est définie dans `.clang-tidy`, axée sur :

- **Sécurité** : Éviter la gestion non sécurisée des tampons et les API dépréciées.
- **Fiabilité** : Détecter les conversions avec rétrécissement et les variables non initialisées.
- **Lisibilité** : Appliquer des styles de codage cohérents et supprimer les « nombres magiques ».
- **Portabilité** : Assurer la conformité aux standards C (CERT, HICPP).

### Préférences de style

Nous privilégions la philosophie « Suckless » :

- Minimiser les dépendances externes.
- **Les commentaires `NOLINT` sont interdits** — corriger la cause racine (voir [Politique anti-suppression](#politique-anti-suppression) ci-dessous).
- Utiliser `static const` ou `enum` plutôt que des nombres magiques.

## Mise en cache incrémentielle (fichiers sentinelles)

À l'origine, nous avons exploré `cltcache`. Cependant, en raison de son surcoût et de sa nécessité de drapeaux de compilateur explicites (`--`), nous sommes passés à un **système de mise en cache basé sur des sentinelles** implémenté directement dans le `Makefile`.

### Fonctionnement

Au lieu de linter chaque fichier à chaque exécution, nous utilisons des « fichiers sentinelles » (`.linted`) pour suivre l'état de chaque fichier source.

1. **Suivi des dépendances** : Chaque fichier `.linted` dans `.lint_cache/` dépend de :
    - Le fichier source `.c` correspondant.
    - La configuration `.clang-tidy` du projet.
    - La base de données `compile_commands.json`.
2. **Comparaison de dates** : `make` compare nativement l'horodatage du source par rapport à la sentinelle. Si le source est plus ancien que la sentinelle, le fichier est ignoré.
3. **Mise à jour** : Si un fichier doit être linté, `clang-tidy` est exécuté. En cas de succès, le fichier sentinelle est mis à jour via `touch`.
4. **Dépendances** : Avant le lint, le système vérifie que les en-têtes générés (comme `glad/glad.h`) sont prêts en construisant les cibles nécessaires.
5. **Parallélisation** : Le processus est parallélisé via `make -j$(NPROCS)`, permettant l'analyse simultanée de plusieurs fichiers.

### Pourquoi cette approche ?

- **Vitesse** : Les exécutions suivantes sont quasi-instantanées (vérification de stat de fichier en O(1)).
- **Robustesse** : Si une analyse est interrompue, la sentinelle n'est pas mise à jour, garantissant qu'elle s'exécute à nouveau au prochain essai.
- **Simplicité** : Aucune dépendance Python externe ni base de données de cache complexe ; exploite le système de fichiers du système d'exploitation et les outils de construction standard.
- **Visibilité** : La sortie du `Makefile` montre clairement quel fichier est en cours de traitement, offrant un retour immédiat.

## Maintenance

Pour vider le cache et forcer un re-lint complet :

```bash
make lint-clean
make lint
```

L'ajout d'une nouvelle règle dans `.clang-tidy` invalidera également automatiquement l'intégralité du cache, garantissant la conformité à l'échelle du projet.

## Hygiène des includes (misc-include-cleaner)

Le check `misc-include-cleaner` est activé dans `.clang-tidy` pour détecter les directives `#include` inutilisées au moment du lint.

### Configuration

```yaml
# .clang-tidy (extrait)
Checks: '...,misc-*,...'
CheckOptions:
  - key: misc-include-cleaner.MissingIncludes
    value: 'false'
```

- **UnusedIncludes** : Activé — signale les headers inclus mais jamais directement utilisés.
- **MissingIncludes** : Désactivé — évite les faux positifs sur les symboles disponibles via des includes transitifs (courant avec cglm, stb, GLFW).

Cela garantit que `just lint` détecte automatiquement les includes obsolètes, sans nécessiter d'outillage spécifique à l'IDE.

## Validation GLSL des shaders

Les shaders sont validés au moment du lint via `glslangValidator` et le script `scripts/lint_shaders.sh`.

### Mode standard (intégré dans `just lint`)

```bash
just lint
# Inclut : clang-tidy + ruff + validation GLSL (26 shaders)
```

Valide tous les shaders `.vert`, `.frag` et `.comp` dans `shaders/`. Le script résout les directives d'inclusion `@header` personnalisées avant de passer le source résolu à `glslangValidator`.

### Mode strict (optionnel, cible SPIR-V)

```bash
just lint-shaders-strict
```

Exécute la validation avec `--target-env opengl` (règles SPIR-V). Ce mode remonte les problèmes comme les qualificateurs `layout(location=N)` manquants, qui empêchent silencieusement le debugger de shaders RenderDoc de fonctionner.

Depuis mars 2026, **les 33 fichiers shader passent la validation SPIR-V stricte**. Le projet impose des `layout(location=N)` explicites sur tous les varyings et uniforms non-opaques, et `layout(binding=N)` sur tous les samplers/images. Voir [renderdoc_guide.fr.md](renderdoc_guide.fr.md#8-debogage-des-shaders-compatibilite-spir-v) pour le détail complet.

## Politique anti-suppression

**`NOLINT`, `NOLINTNEXTLINE` et `NOLINTBEGIN`/`NOLINTEND` sont interdits.** L'approche correcte est toujours de corriger la cause racine.

Un garde automatisé (`scripts/check_nolint.sh`) applique cette règle à chaque étape du workflow de développement :

| Étape | Déclencheur | Mécanisme | Bloquant |
|-------|-------------|-----------|----------|
| **Commit local** | `git commit` sur `*.c` / `*.h` | Hook pre-commit (`check-nolint` dans `.pre-commit-config.yaml`) | Oui (contournable : `--no-verify`) |
| **Manuel local** | `just check-nolint` ou `make check-nolint` | Invocation directe du script | Oui (exit 1) |
| **CI — push** | Push sur n'importe quelle branche | Step du job `lint-and-format` | Oui — fait échouer le job |
| **CI — pull request** | PR ouverte/mise à jour ciblant master | Step du job `lint-and-format` | Oui — fait échouer le job |
| **CI — planifié** | Cron nocturne (01h00 UTC) | Step du job `lint-and-format` | Oui — fait échouer le job |

### Fonctionnement

Le script vérifie les nouveaux ajouts de `NOLINT` dans les fichiers `*.c` et `*.h` avec une approche à deux niveaux :

1. **Changements commités** : `git diff <base_ref>...HEAD` détecte les NOLINT dans le code déjà commité.
2. **Changements stagés** : `git diff --cached <base_ref>` détecte les NOLINT dans le contenu sur le point d'être commité (index). C'est essentiel lors du `pre-commit`, où `HEAD` pointe encore sur le commit précédent et le contenu stagé serait autrement invisible.

Lorsque des fichiers stagés existent, leur contenu final est comparé à la ref de base, gérant correctement les cas où les changements stagés *ajoutent* ou *suppriment* des suppressions NOLINT. Si des lignes NOLINT nettes sont trouvées, le check échoue avec la liste de toutes les violations.

```bash
# Utilisation locale
just check-nolint                    # Compare vs origin/master (défaut)
just check-nolint origin/main        # Ref de base personnalisée
make check-nolint NOLINT_BASE_REF=origin/main
```

### Politique d'exception

Si la suppression est la **seule option viable**, elle nécessite :

1. Un commentaire explicite expliquant pourquoi le fix n'est pas possible.
2. Une évaluation confirmant qu'aucune alternative n'existe.
3. Un ticket de suivi pour revisiter et retirer la suppression.
4. **Validation explicite de l'utilisateur** avant le commit (utiliser `--no-verify`).
