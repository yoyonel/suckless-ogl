# Utiliser Just (lanceur de commandes)

Le projet prend en charge [Just](https://github.com/casey/just) comme alternative moderne à `make`. `Just` fournit un lanceur de commandes pour les tâches spécifiques au projet, offrant une syntaxe plus claire, une meilleure gestion des arguments et des capacités robustes multiplateformes.

## Installation

Pour utiliser `just`, vous devez l'installer. Consultez le [Guide d'installation de Just](https://github.com/casey/just#installation) pour les détails.

Sur la plupart des distributions Linux :

```bash
# Ubuntu/Debian
sudo apt install just

# Fedora
sudo dnf install just

# Arch
sudo pacman -S just
```

## Démarrage rapide

Pour voir toutes les commandes disponibles, exécutez simplement :

```bash
just
# ou
just --list
```

Cela affichera une liste formatée de toutes les recettes avec leurs descriptions.

## Commandes courantes

Voici une correspondance des commandes `make` courantes avec leurs équivalents `just` :

| Tâche | Makefile | Justfile | Notes |
| :--- | :--- | :--- | :--- |
| **Construire** | `make` | `just build` | Construit la version debug par défaut |
| **Exécuter** | `make run` | `just run` | Construit et lance l'application |
| **Nettoyer** | `make clean` | `just clean` | Nettoyage CMake |
| **Nettoyage complet** | `make clean-all` | `just clean-all` | Supprime le répertoire build |
| **Tous les tests** | `make test` | `just test` | Exécute tous les tests |
| **Test spécifique** | `make test/<nom>` | `just test <nom>` | Exécute les tests correspondants |
| **ApiTrace unitaire** | `make test-apitrace` | `just test-apitrace` | Vérification performance légère |
| **ApiTrace intégr.** | `make test-integration-apitrace` | `just test-integration-apitrace` | Test scénario complet |
| **Couverture** | `make coverage` | `just coverage` | Génère le rapport HTML |
| **Lint** | `make lint` | `just lint` | Exécute clang-tidy et ruff |
| **Format** | `make format` | `just format` | Exécute clang-format et ruff |
| **Docs** | `make docs` | `just docs` | Construit MkDocs + Doxygen |
| **Docker Build** | `make docker-build` | `just docker-build` | Construction de production multi-étapes |
| **Docker Run** | `make docker-run` | `just docker-run` | Exécution avec transfert X11 |
| **Docker Prune** | `make docker-clean-all` | `just docker-clean-all` | Nettoie tous les conteneurs/images/cache |
| **Config Windows** | `-` | `just configure-win` | Configure CMake pour cross-compilation MinGW |
| **Build Windows** | `-` | `just build-win` | Construit pour Windows (.exe) |
| **Run Windows** | `-` | `just run-win` | Exécute l'application Windows via Wine |
| **Test Windows** | `-` | `just test-win` | Exécute les tests d'intégration via Wine |
| **Test unitaire Win** | `-` | `just test-win-unit` | Exécute les tests unitaires (CTest) via Wine |

## Utilisation avancée

### Tests

Exécuter un test spécifique de manière interactive avec les journaux en temps réel :

```bash
just test test_app
```

Exécuter un groupe de tests correspondant à un motif :

```bash
just test shader
```

Cette commande :

1. Construit le binaire de test.
2. Si le binaire existe, l'exécute directement (via le wrapper `xvfb`) pour une sortie immédiate.
3. Si introuvable, revient à la correspondance de motif `ctest`.

### Variantes de construction

- **Release** : `just release` / `just run-release`
- **Small** : `just small` / `just run-small` (MinSizeRel)
- **SSBO** : `just build-ssbo` / `just run-ssbo` (Shader Storage Buffer Objects)
- **Sync Debug** : `just build-sync` / `just run-sync` (Vérification synchrone des erreurs GL)
- **ASan** : `just asan` / `just memcheck-asan` (AddressSanitizer)

### Analyse et outils

- **Couverture** : `just coverage` — Construit avec instrumentation et génère un rapport dans `build-coverage/coverage_report/index.html`.
- **Valgrind** : `just memcheck` — Exécute l'application sous Valgrind.
- **Perf** : `just perf` — Exécute le profileur Linux `perf` (nécessite des privilèges).
- **Performance ApiTrace** :
  - `just test-apitrace` : Vérification légère automatisée au niveau test unitaire.
  - `just test-integration-apitrace` : Vérification automatisée de scénario complet avec interaction utilisateur.
  - `just trace-perf` : Analyse manuelle héritée de la trace GPU.

### Gestion des dépendances et environnement

Le `Justfile` détecte automatiquement votre environnement :

- **Distrobox** : Si vous disposez de `distrobox` et du conteneur `clang-dev`, les commandes s'y exécutent automatiquement.
- **Python** : Détecte `uv`, `.venv`, ou `python3` système automatiquement (logique ISO Makefile).
- **Dépendances** : `just deps-setup` télécharge les dépendances hors-ligne via le script.

## Pourquoi Just ?

- **Arguments** : Passer des arguments est natif (`just test pattern` vs `make test/pattern` ou `make test-one TEST=pattern`).
- **Variables** : La logique pour les variables comme l'environnement Python est plus claire.
- **Shell** : Les recettes sont exécutées dans `bash` par défaut, mais peuvent utiliser d'autres shells (ex. : `python`).
- **Découverte** : `just --list` fournit une interface auto-documentée.
