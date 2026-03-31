# Stratégie de lint et mise en cache

Ce document présente la stratégie d'analyse statique du projet `suckless-ogl` et l'implémentation de son mécanisme de mise en cache haute performance.

## Stratégie : Clang-Tidy

Nous utilisons `clang-tidy` pour l'analyse statique. La configuration est définie dans `.clang-tidy`, axée sur :

- **Sécurité** : Éviter la gestion non sécurisée des tampons et les API dépréciées.
- **Fiabilité** : Détecter les conversions avec rétrécissement et les variables non initialisées.
- **Lisibilité** : Appliquer des styles de codage cohérents et supprimer les « nombres magiques ».
- **Portabilité** : Assurer la conformité aux standards C (CERT, HICPP).

### Préférences de style

Nous privilégions la philosophie « Suckless » :

- Minimiser les dépendances externes.
- Éviter les commentaires `NOLINT` sauf si absolument nécessaire (ex. : variables globales pour le contexte de test).
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

Exécute la validation avec `--target-env opengl` (règles SPIR-V). Ce mode remonte les problèmes comme les qualificateurs `layout(location=N)` manquants, pouvant causer des problèmes dans des outils comme le debugger de shaders RenderDoc. Ce mode est informatif et non requis pour les commits.
