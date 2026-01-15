# Build System and Dependency Management

This project uses **CMake** for build automation and dependency management. To facilitate development in atomic OS environments (like Bazzite/Fedora Silverblue), it is designed to run within a **Distrobox** container.

## Build Requirements

- **CMake** 3.14 or higher
- **Clang** or **GCC** (C99 support)
- **Python 3** (required by GLAD for code generation)
- **GLFW 3** (system library)
- **Pkg-config**

## Distrobox Integration

The project is optimized for a container named `clang-dev`. A `Makefile` wrapper is provided to automatically route commands through `distrobox`.

### Common Commands

- `make`: Configures and builds the project inside the container.
- `make run`: Builds and executes the application.
- `make rebuild`: Performs a clean build from scratch (useful when dependencies change).
- `make format`: Formats all C source and header files using **clang-format**.
- `make lint`: Performs static analysis across the codebase using **clang-tidy** (Guaranteed **0 warnings**).
- `make clean-all`: Nukes the `build/` directory.

## Offline Build Support

For development without an internet connection, you can cache dependencies locally.

### 1. Preparation (Online)
Run the following command while you have an active internet connection:
```bash
make deps-setup
```
This script will clone `cglm`, `glad`, `stb`, `unity`, `cjson` into a local `deps/` directory (automatically ignored by Git).

### 2. Building (Offline)
Once the `deps/` directory is populated, CMake will automatically detect it and use the local sources instead of trying to download them. Simply run:
```bash
make
```

To remove the local cache, use `make deps-clean`.

### 3. Testing the Offline Mode
To verify that the build truly doesn't reach the network, you can run it in a simulated offline environment:
```bash
make offline-test
```
*Note: This command sets `http_proxy` to a non-existent local address to block network access. This method is used instead of `unshare -rn` to ensure compatibility with Distrobox/Podman.*

## Dependency Management

Dependencies are automatically managed via CMake's `FetchContent` module. No manual installation or management of `deps/` is required.

### cglm (OpenGL Mathematics for C)
- **Source**: [recp/cglm](https://github.com/recp/cglm)
- **Configuration**: Built as a **static library** (`CGLM_STATIC=ON`).
- **Automation**: Fetched at configuration time.

### GLAD (OpenGL Loader Generator)
- **Source**: [Dav1dde/glad](https://github.com/Dav1dde/glad)
- **Configuration**: Programmatically generates headers and source for **OpenGL 4.4 Core**.
- **Automation**: The generator script is fetched and executed during the CMake configuration phase.

### GLFW
- **Source**: System library.
- **Automation**: Located using `pkg-config`. Ensure `glfw-devel` (or equivalent) is installed in your development container.

### Unity
- **Source**: [ThrowTheSwitch/Unity](https://github.com/ThrowTheSwitch/Unity)
- **Automation**: Fetched at configuration time.

### cJSON
- **Source**: [DaveGamble/cJSON](https://github.com/DaveGamble/cJSON)
- **Automation**: Fetched at configuration time.

## Folder Structure

## Fast Parallel Builds

Le build est optimisé pour être rapide en utilisant la compilation parallèle :

```bash
# Via le wrapper Makefile
make all  # Exécute cmake --build build --parallel

# Directement via CMake
cmake --build build -j$(nproc)
```

## Logging System

L'application utilise un système de logs structuré pour faciliter le développement et le monitoring.

- **Niveau INFO** : Chargement des assets, infos GPU, changements d'état utilisateur.
- **Niveau ERROR** : Échecs de compilation de shader, erreurs d'allocation, échec de création de fenêtre.
- **Niveau WARN** : Détecte les anomalies non critiques.

**Format** : `2026-01-13 10:05:10,133 - module - LEVEL - message`

## Automated Dependencies

Grâce à `FetchContent`, les bibliothèques suivantes sont gérées sans intervention manuelle :

1. **cglm** : Compilé en tant que bibliothèque statique optimisée pour votre CPU.
2. **GLAD** : Généré à la volée pour cibler exactement le profil OpenGL 4.4 Core.
3. **stb_image** : Automatisation via FetchContent. L'implémentation est isolée dans `src/stb_image_impl.c` pour garantir un linting parfaitement propre sur les fichiers sources restants.
4. **unity** : Automatisation via FetchContent.
5. **cjson** : Automatisation via FetchContent.

## Folder Structure

- `CMakeLists.txt` : Cœur du système de build.
- `Makefile` : Raccourcis pour Docker/Distrobox et builds rapides.
- `docs/` : Documentation complète du moteur.
- `src/log.c` : Cœur du système de logging.
- `assets/` : Dossier contenant les assets.
- `assets/env.hdr` : Texture d'environnement source (Equirectangulaire).
- `deps/` : Dossier contenant les dépendances externes.
