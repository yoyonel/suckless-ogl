# CONSTITUTION SUPRÊME DE SUCKLESS-OGL

## DOMAINE I : PARADIGME C11, MÉMOIRE ET ABSTRACTION (PAL)
* [LOCAL:pal][ANCRÉ][ARCHI-REVIEW] L'abstraction Platform Abstraction Layer (PAL) isole de façon étanche toutes les dépendances et appels système natifs (timers, threads, fichiers, handles).
* [LOCAL:pal][ANTI-PATTERN][GREP] Interdiction d'exposer ou de manipuler des structures ou descripteurs de répertoires spécifiques au système (DIR*, HANDLE, etc.) hors de la PAL.
* [GLOBAL][ANTI-PATTERN][GREP] Interdiction d'inclure des en-têtes système natifs (<windows.h>, dirent.h, <sys/...>, <pthread.h>) dans les modules de logique métier, de scène et de rendu hors de l'unité PAL. EXCEPTIONS FORMELLES ET EXCLUSIVES ACCORDÉES AUX INFRASTRUCTURES ORTHOGONALES DE DIAGNOSTIC : log.c, tracy_manager, et les sondes bas-niveau de perf_mode / gpu_usage.
* [GLOBAL][ANCRÉ][COMPILATEUR] Les dépendances de types manipulés uniquement par pointeur dans les en-têtes sont résolues exclusivement par déclarations anticipées (typedef struct X X;).
* [GLOBAL][ANTI-PATTERN][ARCHI-REVIEW] Interdiction d'inclure un fichier d'en-tête complet dans un fichier .h si le type défini n'est manipulé que par pointeur ou par référence.
* [GLOBAL][ANTI-PATTERN][COMPILATEUR] Interdiction de définir les structures de données clés (App, Camera, AsyncRequest, Shader, NBodySim, SphereInstance) sous forme de structures anonymes pour ne pas bloquer les déclarations anticipées.
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] Toute fonction acquérant plusieurs ressources doit utiliser un point de sortie unique marqué par l'étiquette 'cleanup' et atteint via 'goto cleanup'.
* [GLOBAL][ANTI-PATTERN][GREP] Interdiction de libérer des ressources par des appels free() successifs en cascade le long des chemins d'erreur.
* [GLOBAL][ANCRÉ][GREP] Initialiser obligatoirement tous les pointeurs de ressources à NULL lors de leur déclaration pour sécuriser l'exécution inconditionnelle de free(NULL).
* [GLOBAL][ANTI-PATTERN][ARCHI-REVIEW] Interdiction de multiplier les labels de nettoyage dans une même fonction lorsque le cycle de vie des sous-systèmes est géré par la table des descripteurs.
* [GLOBAL][ANCRÉ][COMPILATEUR] La structure App doit être allouée dynamiquement sur le tas avec un alignement SIMD (SIMD_ALIGNMENT) et immédiatement initialisée à zéro via *app = (App){0}.
* [LOCAL:pal][ANCRÉ][COMPILATEUR] L'allocation et la libération de blocs alignés sur les architectures cibles doivent utiliser exclusivement platform_aligned_alloc et platform_aligned_free.
* [LOCAL:pal][ANCRÉ][COMPILATEUR] Le scan de répertoire doit employer l'interface fonctionnelle à callback bool platform_dir_list(const char* path, PlatformDirCallback callback, void* user_data) et son prototype précis typedef void (*PlatformDirCallback)(const char* name, bool is_dir, void* user_data).
* [LOCAL:pal][ANCRÉ][COMPILATEUR] L'acquisition du temps monotonique haute résolution s'effectue via platform_get_time_ns() (type de retour uint64_t) et l'endormissement précis via platform_sleep_ms().
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] Les candidats à l'opacification sur le tas doivent répondre aux métriques : taille > 1 Ko, fréquence d'accès CPU < 200 fois/frame, et absence d'intégration dans des boucles internes serrées.
* [GLOBAL][ANTI-PATTERN][ARCHI-REVIEW] Interdiction d'opacifier des structures légères (< 64 octets), des ressources accédées dans les boucles critiques CPU, ou des handles GPU requis dans la boucle de dessin principale.
* [GLOBAL][ANTI-PATTERN][ARCHI-REVIEW] Interdiction de stocker en ligne des volumes de données froides supérieures à 1 Ko (telles que TrailRenderer de 128 Ko) au sein de structures hôtes chaudes.
* [LOCAL:scene][MVP/TRANSITION][ARCHI-REVIEW] Allouer SceneVisuals (englobant les données de TrailRenderer) sur le tas via le pointeur opaque visuals* pour regrouper les champs shaders* et lighting de la structure Scene sur la même ligne de cache (taille de Scene ~900 octets, max 14 lignes de cache).
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] Décomposer les structures volumineuses comme Scene et PostProcess en sous-structures typées et sous-en-têtes dédiés alignés par domaine fonctionnel.
* [LOCAL:ci][ANCRÉ][COMPILATEUR] Les liaisons de bibliothèques système (m, dl) et les paramètres compilateur (-rdynamic) sous Linux doivent être isolés sous CMake avec des blocs if(UNIX), et CMake doit encapsuler l'exécution des tests unitaires Windows (WIN32) sous Wine via run_test_with_xvfb.sh.

## DOMAINE II : MACHINE D'ÉTAT OPENGL ET BACKEND GRAPHIQUE
* [GLOBAL][ANCRÉ][COMPILATEUR] Support et exécution exclusifs sous OpenGL 4.4 Core Profile, avec configuration systématique du flag GLFW_OPENGL_FORWARD_COMPAT lors de l'initialisation du contexte GLFW.
* [LOCAL:window][ANCRÉ][ARCHI-REVIEW] Initialiser GLFW avec les caractéristiques suivantes : OpenGL 4.4 Core Profile, Debug Context actif (GL_OPENGL_DEBUG_CONTEXT), MSAA désactivé (DEFAULT_SAMPLES = 1).
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] Le sous-système de gestion de la fenêtre graphique (APP_WINDOW_DESCRIPTOR) doit obligatoirement figurer en premier dans la table d'initialisation (APP_SUBSYSTEM_TABLE) pour instancier le contexte OpenGL requis.
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] La libération des sous-systèmes doit s'effectuer dans l'ordre strictement inverse de leur initialisation, le contexte OpenGL devant rester valide (destruction de la fenêtre en dernier lieu).
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] En cas d'échec d'initialisation du sous-système d'index N, le mécanisme de rollback automatique doit libérer les sous-systèmes déjà initialisés de l'index N-1 à 0 dans l'ordre inverse.
* [LOCAL:app][ANTI-PATTERN][GREP] Interdiction de définir un paramètre de taille ou de décompte explicite pour la table des sous-systèmes (terminée par la sentinelle {0}).
* [LOCAL:window][ANTI-PATTERN][ARCHI-REVIEW] Éviter d'exécuter des modifications d'états d'OpenGL ou de réallouer des tampons directement à l'intérieur des callbacks asynchrones de GLFW.
* [LOCAL:window][ANCRÉ][GREP] Désactiver la synchronisation verticale globale lors de l'initialisation de la fenêtre via glfwSwapInterval(0).
* [LOCAL:window][ANCRÉ][ARCHI-REVIEW] Intercepter chaque événement de débogage OpenGL via un callback synchrone (GL_DEBUG_OUTPUT_SYNCHRONOUS) avec déduplication des messages via table de hachage.
* [LOCAL:window][ANCRÉ][ARCHI-REVIEW] Le redimensionnement de la fenêtre doit être différé : le callback de dimensionnement se borne à lever l'indicateur resize_pending, la recréation physique des FBOs et du viewport étant exécutée au début de la frame suivante.
* [LOCAL:renderer][ANCRÉ][ARCHI-REVIEW] Rendre les sphères par défaut via le pattern Billboard Ray-Tracing analytique : projeter un quad unique aligné à l'écran par instance (glDrawArraysInstanced sur 4 sommets), calculer l'intersection rayon-sphère dans le fragment shader, et écrire manuellement la normale et la profondeur projetées (gl_FragDepth).
* [LOCAL:renderer][ANCRÉ][ARCHI-REVIEW] Le rendu d'icosphères maillées instanciées (glDrawElementsInstanced, subdivision restreinte à [0, 6]) est l'unique solution de secours (fallback) lorsque billboard_mode == 0.
* [LOCAL:sorting][ANCRÉ][ARCHI-REVIEW] Effectuer un tri de type back-to-front sur le CPU ou le GPU avant d'appeler le rendu des quads de sphères transparents (Billboard Mode) pour assurer l'exactitude du mélange alpha (modes supportés : CPU_QSORT, CPU_RADIX, GPU_BITONIC).
* [LOCAL:postprocess][ANCRÉ][RUNTIME/PROFILER] Le Framebuffer Object (FBO) principal de la scène doit être configuré en MRT : GL_COLOR_ATTACHMENT0 en GL_RGBA16F (couleur HDR, FXAA), GL_COLOR_ATTACHMENT1 en GL_RG16F (vélocité flou de mouvement), GL_DEPTH_STENCIL_ATTACHMENT en GL_DEPTH32F_STENCIL8 avec Stencil View GL_R8UI.
* [LOCAL:postprocess][ANCRÉ][RUNTIME/PROFILER] Les textures d'entrée du fragment shader composite doivent respecter la liaison stricte aux unités de texture suivantes : 0 (Couleur HDR), 1 (Bloom), 2 (Profondeur), 3 (Auto-exposition), 4 (Vélocité), 5 (Vélocité Neighbor Max), 6 (Flou DoF), 7 (Stencil), 8 (3D LUT).
* [LOCAL:postprocess][ANCRÉ][COMPILATEUR] Caractéristiques métrologiques PP : FBO principal 1920x1080, Bloom 6 niveaux GL_RGBA16F, 3D LUT grille 32x32x32 GL_RGB16F ou GL_RGBA16F, cache LRU de variantes max 32 entrées.
* [LOCAL:ubo][ANCRÉ][COMPILATEUR] Correspondance binaire stricte : la structure C PostProcessUBO_Layout doit correspondre octet par octet au bloc GLSL PostProcessBlock_Layout sous disposition std140 (scalaires sur 4, vec2 sur 8, vec3/vec4/mat4 sur 16, blocs sur frontières de 16 octets).
* [LOCAL:ubo][ANTI-PATTERN][COMPILATEUR] Interdiction d'utiliser des tableaux de padding dans le bloc GLSL std140 ; déclarer les paddings sous forme de scalaires individuels (ex: float _pad1_0; float _pad1_1;).
* [LOCAL:ubo][ANCRÉ][RUNTIME/PROFILER] Mise à jour UBO : transfert unique par frame de l'intégralité du bloc via glBufferSubData en passant une copie locale de la structure C allouée sur la pile. Interdiction d'envoyer les paramètres via des appels glUniform* individuels.
* [LOCAL:async_loader/light_probes][MVP/TRANSITION][GREP] La gestion des threads dans async_loader et light_probes est en cours de migration depuis <pthread.h> direct vers l'abstraction PAL native.

## DOMAINE III : CONCURRENCE, ASYNCHRONISME ET PROGRESSIVITÉ (IBL)
* [LOCAL:async][ANCRÉ][ARCHI-REVIEW] Découpler les threads POSIX : le worker thread E/S gère le chargement disque (stbi_loadf), le décodage et la conversion SIMD F32 vers F16 en RAM CPU libre de tout contexte OpenGL ; le thread de rendu principal gère le contexte OpenGL et les allocations GPU.
* [GLOBAL][ANCRÉ][RUNTIME/PROFILER] Garantir la fluidité du thread principal : aucune tâche bloquante ne doit excéder un budget de 1-2 ms sur le thread de rendu ; déporter les calculs sur le thread worker et diviser le travail GPU lourd sur plusieurs frames.
* [LOCAL:async][ANCRÉ][ARCHI-REVIEW] Protéger tout accès à AsyncRequest par request_mutex ; le worker s'endort sur request_cond si ASYNC_WAITING_FOR_PBO et libère le mutex pendant la conversion SIMD ou le chargement E/S.
* [LOCAL:async][ANTI-PATTERN][ARCHI-REVIEW] Interdiction d'utiliser des files d'attente sans verrou (lock-free ring buffers) personnalisées et non validées à la place de la synchronisation standard POSIX.
* [LOCAL:async][ANTI-PATTERN][COMPILATEUR] Interdiction d'appeler des fonctions de l'API OpenGL depuis le worker thread d'arrière-plan.
* [LOCAL:async][ANTI-PATTERN][ARCHI-REVIEW] Interdiction de manipuler, copier ou convertir des tableaux de pixels CPU directement sur le thread de rendu principal.
* [LOCAL:async][ANCRÉ][RUNTIME/PROFILER] Double-buffering PBO : alterner entre deux PBO persistants (upload_pbo[2] de taille upload_pbo_size[2]) à chaque frame (frame % 2).
* [LOCAL:async][ANCRÉ][RUNTIME/PROFILER] Mapper le PBO sans synchronisation via glMapBufferRange avec GL_MAP_UNSYNCHRONIZED_BIT sur le PBO inutilisé.
* [LOCAL:async][ANTI-PATTERN][GREP] Interdiction de pratiquer le buffer orphaning via glBufferData(..., NULL) sur des tampons de grande taille (64 Mo+).
* [LOCAL:async][ANTI-PATTERN][ARCHI-REVIEW] Interdiction de réaliser l'allocation de stockage (glTexStorage2D) et le transfert DMA (glTexSubImage2D) dans la même frame ; téléverser sur 3 frames (N, N+1, N+M).
* [LOCAL:async][ANCRÉ][COMPILATEUR] Respecter la séquence d'états de requête : ASYNC_IDLE -> ASYNC_PENDING -> ASYNC_LOADING -> ASYNC_WAITING_FOR_PBO -> ASYNC_CONVERTING -> ASYNC_READY -> ASYNC_IDLE/ASYNC_FAILED.
* [LOCAL:sync][ANCRÉ][RUNTIME/PROFILER] Readbacks GPU asynchrones : lire les textures (luminance, histogrammes) dans un PBO double-bufré et insérer un fence glFenceSync ; interroger via glClientWaitSync avec timeout de 0 ns sans bloquer.
* [LOCAL:sync][ANTI-PATTERN][GREP] Interdiction d'appeler glGetError() dans le hot path ou après des appels critiques OpenGL (utiliser GL_DEBUG_OUTPUT), et d'exécuter glReadPixels/glGetTexImage sans PBO ni fence.
* [LOCAL:sync][ANCRÉ][RUNTIME/PROFILER] Sécuriser le basculement plein écran : appeler explicitement glFinish() pour purger le pipeline avant d'appeler glfwSetWindowMonitor().
* [LOCAL:ibl][ANCRÉ][RUNTIME/PROFILER] Cycle d'états de l'IblCoordinator : transition stricte IBL_STATE_IDLE -> IBL_STATE_LUMINANCE -> IBL_STATE_LUMINANCE_WAIT -> IBL_STATE_SPECULAR_INIT -> IBL_STATE_SPECULAR_MIPS -> IBL_STATE_IRRADIANCE -> IBL_STATE_DONE -> IBL_STATE_IDLE (rebouclage permis après consommation des résultats par ibl_coordinator_get_results pour le rechargement à chaud).
* [LOCAL:ibl][ANCRÉ][RUNTIME/PROFILER] Slicing temporel IBL : sur GPU matériel, Specular Mip 0 en 24 tranches, Mip 1 en 8 tranches, Mip 2 en 1 tranche dédiée, Mips 3+ regroupées sur 1 frame (tail grouping), Irradiance en 12 tranches, Luminance en 2 dispatches. Limite de 10-15 ms par tranche.
* [LOCAL:ibl][ANCRÉ][ARCHI-REVIEW] Détecter les GPU logiciels (llvmpipe) et désactiver le slicing (1 dispatch global par carte) sauf pour la réduction de luminance (2 dispatches).
* [GLOBAL][ANCRÉ][RUNTIME/PROFILER] Interdiction d'afficher la scène 3D ou d'initier le fade in (l'écran reste noir, transition_alpha = 1.0) avant que l'IBLCoordinator n'ait atteint l'état IBL_STATE_DONE.
* [LOCAL:ibl][ANCRÉ][GREP] Injecter les limites Y de la tranche active (u_max_y_slice, u_offset_y) aux compute shaders (32x32) ; interdiction de lancer le compute sans filtre de garde (pixel_pos.y >= u_max_y_slice).
* [LOCAL:ibl][ANCRÉ][GREP] Appeler un unique glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) à l'état final IBL_STATE_DONE ; interdiction d'insérer des barrières à la fin de chaque tranche.
* [LOCAL:ibl][ANCRÉ][ARCHI-REVIEW] Transfert de propriété obligatoire via ibl_coordinator_get_results et mise à NULL immédiate des handles internes pour éviter les double-libérations.

## DOMAINE IV : HYGIÈNE DU CODE, MÉTROLOGIE ET CONVENTIONS
* [GLOBAL][MVP/TRANSITION][GREP] Limite de 500 lignes de code (LOC) par module ; un module dépassant 500 LOC doit être décomposé en unités spécialisées.
* [GLOBAL][ANCRÉ][GREP] Limite stricte de 150 lignes effectives (LOC) par fonction (hors commentaires et macros) ; viser < 100 LOC pour les fonctions d'orchestration principales (scene_render, fx_bloom_render).
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] Include fan-out maximum : interdiction de concevoir des en-têtes avec plus de 5 inclusions d'en-têtes de projet. Déporter les inclusions vers le .c et utiliser des déclarations anticipées.
* [LOCAL:pp][ANTI-PATTERN][COMPILATEUR] Interdiction d'inclure plus de 8 en-têtes de projet dans l'en-tête interne postprocess_internal.h (isoler l'état dans pp_effects_state.h).
* [GLOBAL][ANCRÉ][GREP] Préfixer toutes les fonctions internes et partagées d'un module par son trigramme (ex: pp_ pour le post-traitement) et les fonctions publiques par son identifiant (ex: app_*).
* [GLOBAL][ANCRÉ][GREP] Suffixes obligatoires : _subsys_init et _subsys_cleanup pour les fonctions de cycle de vie des sous-systèmes.
* [GLOBAL][MVP/TRANSITION][RUNTIME/PROFILER] Le budget nominal de mémoire vidéo (VRAM) au repos est borné à 101 Mo : Textures ~99 Mo, Buffers ~40 Ko, Shaders compilés ~2 Mo.
* [GLOBAL][ANCRÉ][RUNTIME/PROFILER] Conformité stricte aux règles clang-tidy sans aucun avertissement de compilation.
* [LOCAL:dependencies][ANCRÉ][ARCHI-REVIEW] Le module de rendu bas niveau (renderer) doit exécuter le rendu de l'interface utilisateur via le type opaque RenderUIFn (typedef void (*RenderUIFn)(void* user_data)) et la fonction trampoline app_render_ui_trampoline.
* [GLOBAL][ANCRÉ][COMPILATEUR] La structure SphereInstance doit mesurer exactement 128 octets, être alignée sur 64 octets (via attribut d'alignement matériel), et respecter son layout strict sans padding explicite erroné (mat4 model, vec3 albedo, float metallic, float roughness, float ao, float padding, vec3 prev_center, le compilateur gérant le padding de fin pour atteindre 128 octets).
* [LOCAL:scene][ANCRÉ][RUNTIME/PROFILER] La scène nominale est constituée d'une grille 2D de 10x10 = 100 sphères, plan Z = 0, espacement de 2.5 unités, dimensions globales de 22.5 x 22.5.
* [LOCAL:camera][ANCRÉ][RUNTIME/PROFILER] Paramètres d'initialisation de l'orbite de la caméra : distance = 20.0, lacet = -90.0°, tangage = 0.0°, FOV vertical = 60.0°, et plans de troncature Z = [0.1, 1000.0].
* [LOCAL:input][ANCRÉ][GREP] Raccourcis clavier obligatoires : C (caméra), SPACE (caméra pos), W (fil de fer), Up/Down (subdivisions), PageUp/PageDown (LOD IBL), F (plein écran), ESC (fermeture).
