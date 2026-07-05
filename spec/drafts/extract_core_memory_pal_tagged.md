# Substance Contraignante : Gestion de Mémoire, PAL, et Localité du Cache

## 1. INVARIANTS ARCHITECTURAUX ET FLUX D'EXÉCUTION
* [LOCAL:pal][ANCRÉ][ARCHI-REVIEW] L'abstraction Platform Abstraction Layer (PAL) isole de façon étanche toutes les dépendances et appels système natifs (timers, threads, fichiers, IDs).
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] L'initialisation automatique gère les rollbacks inversés `[N-1 .. 0]` pour tout échec d'index `N` dans la table de descripteurs de sous-systèmes.
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] Seuls les échecs de Phase 3 postérieurs à l'initialisation des descripteurs (scene, async loader, post-processing) effectuent un saut vers `goto cleanup_full`.
* [GLOBAL][ANCRÉ][RUNTIME/PROFILER] La structure `App` réside en ligne sur la pile du thread principal (`main`).
* [GLOBAL][ANCRÉ][RUNTIME/PROFILER] Les champs de structures chauds co-accédés par frame doivent être situés dans la même ligne de cache (64 octets) ou dans une proximité maximale de 1 à 2 lignes.
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] L'opacification via pointeur opaque sur le tas est tolérée uniquement si le surcoût d'indirection global reste négligeable (~900 ns/frame pour 16,6 ms de budget).
* [GLOBAL][ANCRÉ][COMPILATEUR] Les dépendances de types manipulés uniquement par pointeur dans les en-têtes sont résolues exclusivement par déclarations anticipées (`typedef struct X X;`).

## 2. INTERDICTIONS FORMELLES ET ANTI-PATTERNS
* [GLOBAL][ANTI-PATTERN][GREP] Interdiction de libérer des ressources par des appels `free()` successifs en cascade le long des chemins d'erreur.
* [GLOBAL][ANTI-PATTERN][GREP] Interdiction d'inclure des en-têtes système natifs (`<windows.h>`, `dirent.h`, `<sys/...>`) dans les modules de l'application cliente hors de l'unité PAL.
* [LOCAL:pal][ANTI-PATTERN][GREP] Interdiction d'exposer ou de manipuler des structures ou descripteurs de répertoires spécifiques au système (`DIR*`, `HANDLE`) hors de la PAL.
* [GLOBAL][ANTI-PATTERN][ARCHI-REVIEW] Interdiction d'opacifier des structures légères (< 64 octets), des ressources accédées dans les boucles critiques CPU, ou des handles GPU requis dans la boucle de dessin principale.
* [GLOBAL][ANTI-PATTERN][ARCHI-REVIEW] Interdiction de stocker en ligne des volumes de données froides supérieures à 1 Ko (telles que `TrailRenderer` de 128 Ko) au sein de structures hôtes chaudes.
* [LOCAL:app][ANTI-PATTERN][GREP] Interdiction de laisser le flag `GLFW_AUTO_ICONIFY` à `GLFW_TRUE` lors de la création de la fenêtre pour éviter la perte de focus sous Wine.
* [GLOBAL][ANTI-PATTERN][ARCHI-REVIEW] Interdiction de multiplier les labels de nettoyage dans une même fonction lorsque le cycle de vie des sous-systèmes est géré par la table des descripteurs.
* [LOCAL:pp][ANTI-PATTERN][COMPILATEUR] Interdiction d'inclure plus de 8 en-têtes de projet dans l'en-tête interne `postprocess_internal.h`.

## 3. PATTERNS OBLIGATOIRES ET GESTION DES RESSOURCES
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] Toute fonction acquérant plusieurs ressources doit utiliser un point de sortie unique marqué par l'étiquette `cleanup` et atteint via `goto cleanup`.
* [GLOBAL][ANCRÉ][GREP] Initialiser obligatoirement tous les pointeurs de ressources à `NULL` lors de leur déclaration pour sécuriser l'exécution inconditionnelle de `free(NULL)`.
* [LOCAL:pal][ANCRÉ][COMPILATEUR] Le scan de répertoire doit employer l'interface fonctionnelle à callback `bool platform_dir_list(const char* path, PlatformDirCallback callback, void* user_data)`.
* [LOCAL:pal][ANCRÉ][COMPILATEUR] L'allocation et la libération de blocs alignés sur les architectures cibles doivent utiliser exclusivement `platform_aligned_alloc` et `platform_aligned_free`.
* [LOCAL:pal][ANCRÉ][COMPILATEUR] L'acquisition du temps monotonique haute résolution s'effectue via `platform_get_time_ns()` et l'endormissement précis via `platform_sleep_ms()`.
* [LOCAL:ci][ANCRÉ][COMPILATEUR] Le système CMake doit encapsuler l'exécution des tests unitaires Windows (WIN32) via le wrapper `run_test_with_xvfb.sh` sous Wine.
* [LOCAL:app][ANCRÉ][GREP] La reprise de focus souris sous Wine lors du passage plein écran requiert un basculement de mode curseur de `GLFW_CURSOR_NORMAL` vers `GLFW_CURSOR_DISABLED` suivi de `glfwSetCursorPos()`.
* [LOCAL:scene][MVP/TRANSITION][ARCHI-REVIEW] Allouer `SceneVisuals` (englobant les données de `TrailRenderer`) sur le tas via le pointeur opaque `visuals*` pour regrouper les champs `shaders*` et `lighting` de la structure `Scene` sur la même ligne de cache.
* [LOCAL:pp][ANCRÉ][GREP] Isoler les structures d'effets dans un en-tête d'état `pp_effects_state.h` dédié pour décharger `postprocess_internal.h`.

## 4. CONVENTIONS DE NOMMAGE, TYPES ET MÉTROLOGIE DU CODE
* [LOCAL:pal][ANCRÉ][COMPILATEUR] Le prototype du callback de parcours de répertoires doit respecter précisément : `typedef void (*PlatformDirCallback)(const char* name, bool is_dir, void* user_data)`.
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] Les candidats à l'opacification sur le tas doivent répondre aux métriques : taille > 1 Ko, fréquence d'accès CPU < 200 fois/frame, et absence d'intégration dans des boucles internes serrées (par sommets ou par particules).
* [LOCAL:pal][ANCRÉ][COMPILATEUR] Les fonctions de timing monotonic doivent retourner un type `uint64_t` représentant des nanosecondes.
* [LOCAL:ci][ANCRÉ][COMPILATEUR] Les liaisons de bibliothèques système (`m`, `dl`) et les paramètres compilateur (`-rdynamic`) sous Linux doivent être isolés sous CMake avec des blocs `if(UNIX)`.
* [LOCAL:scene][ANCRÉ][ARCHI-REVIEW] La structure `Scene` après opacification de `SceneVisuals` doit mesurer environ 900 octets et occuper au maximum 14 lignes de cache.
