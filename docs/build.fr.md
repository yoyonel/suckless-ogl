# Guide de compilation

Ce document décrit le processus de compilation du projet `suckless-ogl`.

## Prérequis

### Outils système

```bash
# Debian/Ubuntu
sudo apt install cmake make clang git

# Outils X11 pour GLFW (compilé depuis les sources)
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

### Compilation avec Distrobox (recommandé)

Distrobox permet d'utiliser un environnement Debian stable même sur des distributions hôtes exotiques (Bazzite, Fedora Silverblue, etc.) :

```bash
# Créer la box de développement
distrobox create --name clang-dev --image debian:13

# Entrer dans la box
distrobox enter clang-dev
```

## Dépendances gérées via FetchContent

Ces bibliothèques sont téléchargées et compilées automatiquement par CMake :

| Bibliothèque | Usage |
|-------------|-------|
| **cglm** | Mathématiques vectorielles et matricielles |
| **GLAD** | Chargeur des extensions OpenGL |
| **GLFW** | Fenêtrage et gestion des entrées |
| **Unity** | Framework de tests unitaires |
| **cJSON** | Parsing JSON pour la configuration |

## Commandes de compilation

### Construction standard

```bash
# Configuration
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Compilation
cmake --build build

# Ou via Just (recommandé)
just build
```

### Variantes

```bash
just release        # Mode Release (optimisé)
just asan           # Avec AddressSanitizer
just build-sync     # Avec vérifications GL synchrones
```

## Drapeaux de sécurité

Le projet compile avec des drapeaux hardening activés en Release :

```cmake
-D_FORTIFY_SOURCE=2   # Protection des tampons libc
-fstack-protector-strong  # Canary de pile
-Wformat=2            # Avertissements de format stricts
-Wl,-z,relro          # Sections relocatables en lecture seule
-Wl,-z,now            # Résolution des symboles au démarrage
```

## Système de journalisation

Le niveau de journalisation est configurable à la compilation via `LOG_LEVEL` :

| Niveau | Valeur | Description |
|--------|--------|-------------|
| `TRACE` | 0 | Tous les messages |
| `DEBUG` | 1 | Messages de débogage |
| `INFO` | 2 | Messages informatifs (défaut) |
| `WARN` | 3 | Avertissements uniquement |
| `ERROR` | 4 | Erreurs uniquement |

```bash
cmake -B build -DLOG_LEVEL=DEBUG
```

## Tests

```bash
just test         # Tous les tests
just test test_app  # Test spécifique
```

## Voir aussi

- [docker.md](./docker.md) — Construction via Docker
- [ide_setup.md](./ide_setup.md) — Configuration de l'IDE
- [justfile.md](./justfile.md) — Référence des recettes Just
