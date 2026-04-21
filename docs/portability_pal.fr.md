# Couche d'abstraction de portabilité (PAL)

Ce document décrit les composants de la couche de portabilité qui permettent au projet de fonctionner sur Linux, macOS et Windows.

## Vue d'ensemble

La PAL (Portability Abstraction Layer) est organisée en trois modules dans `src/platform/` :

| Module | Fichier | Responsabilité |
|--------|---------|---------------|
| `platform_utils` | `platform_utils.c/.h` | Détection de plateforme, macros |
| `platform_time` | `platform_time.c/.h` | Mesure du temps haute résolution |
| `platform_fs` | `platform_fs.c/.h` | Opérations sur le système de fichiers |

## Composant : platform_utils

### Détection de plateforme

```c
#if defined(_WIN32)
    #define PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define PLATFORM_MACOS 1
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
#endif
```

### Macros utilitaires

```c
// Attributs portables
#define MAYBE_UNUSED __attribute__((unused))
#define INLINE       static inline __attribute__((always_inline))

// Séparateur de chemin
#ifdef PLATFORM_WINDOWS
    #define PATH_SEP '\\'
#else
    #define PATH_SEP '/'
#endif
```

## Composant : platform_time

Fournit un chronomètre haute résolution indépendant de la plateforme :

```c
typedef struct { uint64_t value; } PlatformTime;

PlatformTime platform_time_now(void);
double platform_time_elapsed_ms(PlatformTime start, PlatformTime end);
double platform_time_elapsed_s(PlatformTime start, PlatformTime end);
```

Implémentation :
- **Linux** : `clock_gettime(CLOCK_MONOTONIC)`
- **macOS** : `mach_absolute_time()`
- **Windows** : `QueryPerformanceCounter()`

## Composant : platform_fs

### Listage de répertoires basé sur le callback

L'API de listage de répertoires utilise une architecture callback pour éviter l'allocation dynamique et permettre un traitement streaming :

```c
typedef void (*fs_dir_entry_callback)(const char* path, void* user_data);

int platform_fs_list_dir(const char* dir,
                         fs_dir_entry_callback callback,
                         void* user_data);
```

Usage :

```c
// Lister tous les fichiers .hdr dans le dossier assets
void on_hdr_found(const char* path, void* ctx) {
    if (str_ends_with(path, ".hdr")) {
        add_environment(ctx, path);
    }
}

platform_fs_list_dir("assets/environments", on_hdr_found, &env_list);
```

### Opérations courantes

```c
// Vérifier l'existence d'un fichier
bool platform_fs_exists(const char* path);

// Obtenir la taille d'un fichier
int64_t platform_fs_size(const char* path);

// Lire un fichier entier en mémoire
uint8_t* platform_fs_read_all(const char* path, size_t* out_size);
```

## Support Windows (cross-compilation)

La compilation Windows s'effectue via la toolchain MinGW :

```bash
# Configurer pour Windows
just configure-win

# Compiler
just build-win

# Exécuter via Wine
just run-win
```

La toolchain est définie dans `toolchain-mingw.cmake` :

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CROSSCOMPILING_EMULATOR "wine64")
```

`CMAKE_CROSSCOMPILING_EMULATOR` est le mécanisme standard CMake pour
l'exécution de tests cross-compilés : CTest préfixe automatiquement
chaque exécutable de test avec `wine64`, sans nécessiter de wrapper
manuel ou de variable d'environnement.

## Voir aussi

- [build.md](./build.md) — Guide de compilation
- [project_structure.md](./project_structure.md) — Structure du projet
