# Documentation : Profilage de Concurrence avec ThreadSanitizer (TSan)

> **Date de mise à jour :** 2026-06-20 16:54:25 CEST
> **Contexte :** Moteur de rendu OpenGL 4.x (PBR+IBL) en C11 avec chargements asynchrones.

## 1. Introduction

Dans le cadre du développement de notre architecture de chargement asynchrone (textures KTX, HDR, génération de mipmaps, IBL), la gestion des `pthreads` introduit des risques critiques de *Data Races* et de *Deadlocks*.

Pour garantir la robustesse du moteur, nous utilisons **ThreadSanitizer (TSan)**, un outil d'instrumentation intégré à GCC/Clang. TSan surveille l'exécution au runtime et signale toute violation d'accès concurrent à la mémoire ou tout risque d'interblocage sur les mutex.

---

## 2. Configuration de la Compilation (CMake)

TSan nécessite une instrumentation du code source et de l'éditeur de liens. L'activation se fait de manière conditionnelle via notre `CMakeLists.txt` pour ne pas impacter les performances des builds standards.

### 2.1. CMakeLists.txt

Une option basculable `ENABLE_TSAN` est ajoutée au projet :

```cmake
option(ENABLE_TSAN "Activer ThreadSanitizer pour l'analyse de concurrence" OFF)

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer est ACTIVÉ")

    # -fsanitize=thread : Active l'instrumentation
    # -g : Obligatoire pour mapper les adresses mémoire aux lignes de code
    # -O1 : Recommandé pour une pile d'appels (stack trace) claire
    target_compile_options(app PRIVATE -fsanitize=thread -g -O1)
    target_link_options(app PRIVATE -fsanitize=thread)

    # Requis par TSan pour les exécutables Linux modernes
    set_target_properties(app PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()
```

### 2.2. Automatisation via Justfile

Pour faciliter le lancement, une cible dédiée est configurée dans le gestionnaire de tâches :

```makefile
# Build and Run with ThreadSanitizer
run-tsan:
    @echo "Building and running with ThreadSanitizer..."
    @mkdir -p build-tsan
    @{{ distrobox }} cmake -G "Unix Makefiles" -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -DENABLE_UNITY_BUILD=OFF
    @{{ distrobox }} cmake --build build-tsan --parallel {{ nprocs }}
    @{{ distrobox }} env TSAN_OPTIONS="suppressions=tsan_suppressions.txt color=always" ./build-tsan/app
```

### 2.3. Résolution des dépendances en conteneur (Distrobox)

Lors de la compilation et du linkage dans un environnement conteneurisé minimal (comme l'environnement de développement `clang-dev` basé sur Fedora), l'éditeur de liens peut échouer avec l'erreur suivante :
```text
/usr/bin/ld: cannot find /usr/lib64/libtsan.so.2.0.0: No such file or directory
```

**Cause :**
Le paquet `gcc` fournit le fichier d'entrée de liaison `/usr/lib/gcc/x86_64-redhat-linux/<version>/libtsan.so` (qui est un script d'éditeur de liens de type linker script contenant `INPUT ( /usr/lib64/libtsan.so.2.0.0 )`), mais la bibliothèque d'exécution ThreadSanitizer elle-même (`libtsan`) n'est pas installée par défaut dans le conteneur.

**Résolution :**
Il est nécessaire d'installer explicitement la dépendance d'exécution à l'intérieur du conteneur `distrobox` :
```bash
distrobox enter clang-dev -- sudo dnf install -y libtsan
```

---

## 3. Paramétrage Fin : Le Fichier de Suppressions

L'écosystème graphique sous Linux (X11, XCB, Mesa/Gallium) génère un grand nombre de "faux positifs" ou d'erreurs historiques de concurrence qui ne concernent pas notre application et polluent la sortie console.

### 3.1. Le piège de `called_from_lib` et l'erreur 66
Initialement, la suppression par le mot-clé `called_from_lib` a été testée. Cependant, OpenGL décharge dynamiquement les pilotes Mesa à la fermeture de l'application (via `dlclose()`). TSan, essayant de surveiller une bibliothèque soudainement disparue de la mémoire, provoquait un crash avec le code d'erreur `Exit Code 66`.

### 3.2. Solution : Filtrage par typologie d'erreur
Nous avons donc opté pour un filtrage explicite des catégories d'erreurs (`race`, `mutex`, `deadlock`) ciblant les bibliothèques problématiques. Cela permet à TSan de survivre au déchargement de la mémoire tout en garantissant un terminal silencieux.

### 3.3. Contenu de `tsan_suppressions.txt`

Ce fichier doit être placé à la racine du projet (au même niveau que l'exécutable ou injecté via la variable d'environnement `$TSAN_OPTIONS`).

```text
# ===================================================================
# Fichier de suppressions ThreadSanitizer (TSan)
# Objectif : Ignorer le "bruit" des pilotes graphiques et du serveur X
# ===================================================================

# --- PILOTES MESA / GALLIUM (Génération de shaders, cache disque) ---
# Ignore les écritures/lectures concurrentes internes au pilote
race:libgallium
race:libGLX_mesa
race:libGLX

# Ignore les manipulations suspectes de mutex (ex: déverrouillage d'un mutex non verrouillé)
mutex:libGLX_mesa
mutex:libgallium

# --- SERVEUR D'AFFICHAGE X11 / XCB ---
# Ignore les data races lors de la réception des événements fenêtrés
race:libxcb
race:libX11

# Ignore les inversions d'ordre de verrouillage (risques de deadlocks théoriques)
deadlock:libX11
deadlock:libGLX
deadlock:libxcb
```

---

## 4. Exploitation et Bonnes Pratiques

Lorsque la cible `run-tsan` est lancée avec ce fichier de suppression, **toute erreur affichée par TSan est un bug critique avéré** dans notre base de code C11.

### Comment analyser un rapport TSan légitime
1. **Identifier l'infraction :** Le rapport commence par le type de problème (ex: `WARNING: ThreadSanitizer: data race`).
2. **Identifier l'Action 1 :** TSan pointe la première instruction fautive (ex: *Write by thread T5*) avec le numéro de ligne exact dans notre fichier source.
3. **Identifier l'Action 2 :** TSan pointe l'événement concurrentiel (ex: *Previous write by main thread*).
4. **Correction :** La solution exige systématiquement de protéger l'accès à la ressource partagée.

**Exemple historique de correction :**
TSan a permis d'identifier une Data Race silencieuse dans notre système de journalisation (`log.c`). Le thread principal et le worker asynchrone appelaient simultanément `localtime()`, dont le buffer statique interne (glibc) n'est pas *thread-safe*.
* **Résolution :** Utilisation de l'alternative POSIX `localtime_r` allouant la structure sur la pile (stack) locale du thread, couplée à un mutex `PTHREAD_MUTEX_INITIALIZER` encadrant l'appel `fputs` pour garantir l'atomicité de l'affichage console.
