# Spécifications d'Architecture et Contraintes Techniques (suckless-ogl)

Ce document rassemble la substance contraignante extraite du cycle de vie de l'application `suckless-ogl`, exempte de toute prose descriptive ou pédagogique.

---

## 1. INVARIANTS ET SÉQUENCEMENT TEMPOREL

[GLOBAL] [ANCRÉ] [ARCHI-REVIEW] Main() doit exécuter le bootstrap dans l'ordre chronologique strict : 1. `tracy_manager_init_global()` (initialisation du profiler), 2. `cli_handle_args()` (analyse des arguments), 3. Allocation de la structure `App` alignée, 4. `app_init()`, 5. `app_run()` (boucle principale), 6. `app_cleanup()`, 7. Libération de `App`.
[GLOBAL] [ANCRÉ] [COMPILATEUR] La structure `App` doit être allouée dynamiquement sur le tas avec un alignement SIMD (`SIMD_ALIGNMENT`) et immédiatement initialisée à zéro via `*app = (App){0}`.
[LOCAL:async_loader] [ANCRÉ] [ARCHI-REVIEW] Le thread de rendu principal doit allouer, mapper le PBO (`glMapBuffer`), puis déléguer l'écriture au thread de chargement asynchrone via `async_loader_provide_pbo()`.
[LOCAL:async_loader] [ANCRÉ] [ARCHI-REVIEW] Le thread POSIX d'Async Loader doit exécuter la lecture disque (`stbi_loadf`) et la conversion float32 vers float16 en arrière-plan, puis notifier le thread principal via l'état `READY` pour permettre le démappage (`glUnmapBuffer`) et le transfert DMA (`glTexSubImage2D`) sur le thread principal.
[LOCAL:async_loader] [ANCRÉ] [RUNTIME/PROFILER] La synchronisation asynchrone d'auto-exposition entre CPU et GPU doit être gérée via une double mise en mémoire tampon de PBO et des barrières de synchronisation GLSync (`glFenceSync`).
[LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] L'IblCoordinator doit transiter séquentiellement à travers les 7 états de sa machine d'état : `IBL_STATE_IDLE` -> `IBL_STATE_LUMINANCE` -> `IBL_STATE_LUMINANCE_WAIT` -> `IBL_STATE_SPECULAR_INIT` -> `IBL_STATE_SPECULAR_MIPS` -> `IBL_STATE_IRRADIANCE` -> `IBL_STATE_DONE`.
[LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] La génération des textures IBL sur GPU matériel doit être découpée temporellement par tranche (slice) sur plusieurs frames consécutives : Specular Mip 0 en 24 tranches, Specular Mip 1 en 8 tranches, Specular Mips 2 à 4 regroupés sur 1 frame, Irradiance Map en 12 tranches, Luminance en 2 dispatches.
[LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] Sur GPU logiciel (llvmpipe), le découpage temporel des calculs IBL doit être désactivé pour exécuter chaque carte en une seule tranche (1 dispatch global), à l'exception de la réduction de luminance qui conserve ses 2 dispatches.
[LOCAL:window] [ANCRÉ] [ARCHI-REVIEW] Initialiser GLFW avec les caractéristiques suivantes avant toute création de ressource GPU : OpenGL 4.4 Core Profile, Debug Context actif (`GL_OPENGL_DEBUG_CONTEXT`), MSAA désactivé (`DEFAULT_SAMPLES = 1`).
[LOCAL:window] [ANCRÉ] [ARCHI-REVIEW] Le sous-système de fenêtrage doit être initialisé en première position dans la table des descripteurs de sous-systèmes de l'application.
[LOCAL:window] [ANCRÉ] [GREP] Désactiver la synchronisation verticale globale lors de l'initialisation de la fenêtre via `glfwSwapInterval(0)`.
[LOCAL:window] [ANCRÉ] [ARCHI-REVIEW] Intercepter chaque événement de débogage OpenGL via un callback synchrone (`GL_DEBUG_OUTPUT_SYNCHRONOUS`) avec déduplication des messages via table de hachage.
[LOCAL:window] [ANCRÉ] [ARCHI-REVIEW] Le redimensionnement de la fenêtre doit être différé : le callback de dimensionnement GLFW se borne à lever un indicateur (`resize_pending`), le redimensionnement physique des FBOs et du viewport étant exécuté au début de la frame suivante.
[LOCAL:camera] [ANCRÉ] [ARCHI-REVIEW] L'intégration de la physique de la caméra doit s'effectuer à pas de temps fixe (60 Hz) via un accumulateur de temps CPU.
[LOCAL:camera] [ANCRÉ] [ARCHI-REVIEW] Le lissage de la rotation de la caméra doit être recalculé à chaque frame par interpolation exponentielle (`rotation_smoothing`).

---

## 2. INTERDICTIONS FORMELLES

[GLOBAL] [ANCRÉ] [RUNTIME/PROFILER] Interdiction d'afficher la scène 3D ou d'initier le fondu d'entrée (fade in) avant que le coordinateur IBL n'ait atteint l'état `IBL_STATE_DONE` (l'écran doit rester opaque noir, `transition_alpha = 1.0`).
[GLOBAL] [ANCRÉ] [ARCHI-REVIEW] Interdiction d'effectuer des lectures de fichiers synchrones ou de l'I/O bloquant sur le thread de rendu principal.
[LOCAL:postprocess] [ANCRÉ] [COMPILATEUR] Interdiction d'accéder à la structure globale `App` au sein des implémentations individuelles des effets de post-processing ; tous les paramètres doivent transiter par le découplage de la structure `EffectContext`.
[LOCAL:shader] [ANCRÉ] [RUNTIME/PROFILER] Interdiction de compiler ou de lier des programmes de shaders dans la boucle de rendu nominale, en dehors des cas de recompilation paresseuse de variantes de post-processing gérées par le cache LRU.
[LOCAL:postprocess] [ANCRÉ] [GREP] Interdiction d'employer des structures de contrôle conditionnelles dynamiques (`if`/`else` sur uniforms) dans le shader composite final pour activer/désactiver des effets ; utiliser exclusivement des directives de précompilation `#ifdef` combinées avec le cache de variantes.
[LOCAL:shader] [ANCRÉ] [RUNTIME/PROFILER] Interdiction de dépasser une profondeur de récursion d'inclusion de 16 niveaux lors du traitement de la directive `@header` par le compilateur de shaders.
[LOCAL:async_loader] [ANCRÉ] [RUNTIME/PROFILER] Interdiction pour le thread asynchrone d'écrire dans la mémoire tampon avant que le thread principal n'ait verrouillé et fourni l'accès au pointeur du PBO.
[LOCAL:window] [ANTI-PATTERN] [ARCHI-REVIEW] Éviter d'exécuter des modifications d'états d'OpenGL ou de réallouer des tampons directement à l'intérieur des callbacks asynchrones de GLFW (tels que le changement de taille de la fenêtre).

---

## 3. PATTERNS OBLIGATOIRES ET ARCHITECTURE GPU

[LOCAL:renderer] [ANCRÉ] [ARCHI-REVIEW] Rendre les sphères par défaut via le pattern Billboard Ray-Tracing analytique : projeter un quad unique aligné à l'écran par instance (`glDrawArraysInstanced` sur 4 sommets), calculer l'intersection rayon-sphère dans le fragment shader, et écrire manuellement la normale et la profondeur projetées (`gl_FragDepth`).
[LOCAL:renderer] [ANCRÉ] [ARCHI-REVIEW] Le rendu d'icosphères maillées instanciées (`glDrawElementsInstanced`) doit être utilisé comme unique solution de secours (fallback) lorsque `billboard_mode == 0`.
[LOCAL:postprocess] [ANCRÉ] [RUNTIME/PROFILER] Le Framebuffer Object (FBO) principal de la scène doit être configuré en MRT (Multiple Render Targets) avec la topologie suivante :
  * `GL_COLOR_ATTACHMENT0` : Format `GL_RGBA16F` (Canaux RGB pour la couleur HDR, canal Alpha pour la luminance requise par l'FXAA).
  * `GL_COLOR_ATTACHMENT1` : Format `GL_RG16F` (Vecteurs de vélocité par pixel pour le flou de mouvement).
  * `GL_DEPTH_STENCIL_ATTACHMENT` : Format `GL_DEPTH32F_STENCIL8` (Z-buffer et masque stencil de segmentation).
  * Stencil View : TextureView non-signée `GL_R8UI` pointant sur le stencil.
[LOCAL:postprocess] [ANCRÉ] [RUNTIME/PROFILER] Les textures d'entrée du fragment shader composite de post-processing doivent respecter la liaison stricte aux unités de texture (Texture Units) suivantes :
  * Unité 0 : Couleur de scène HDR (FBO Color 0)
  * Unité 1 : Tampon de Bloom (Composite de Bloom)
  * Unité 2 : Profondeur de scène (FBO Depth)
  * Unité 3 : Valeur d'auto-exposition (Luminance)
  * Unité 4 : Vélocité (FBO Color 1)
  * Unité 5 : Vélocité Neighbor Max (Flou de mouvement)
  * Unité 6 : Flou DoF (Depth of Field)
  * Unité 7 : Masque Stencil (Stencil View)
  * Unité 8 : Look-Up Table 3D (Fichier `.cube` de Color Grading)
[LOCAL:shader] [ANCRÉ] [GREP] Gérer l'inclusion de fichiers de shaders via la syntaxe custom `@header "nom_fichier.glsl"`, avec résolution récursive des dépendances et prévention des inclusions multiples par gardes d'inclusion virtuels.
[LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] Lier des textures sentinelles 1x1 (`dummy_black_tex` ou `dummy_white_tex`) sur les unités IBL actives pour prévenir les lectures indéfinies sur GPU en l'absence de carte HDR environnementale chargée.
[LOCAL:ibl] [ANCRÉ] [ARCHI-REVIEW] Générer la table de BRDF LUT une seule fois lors de la phase d'initialisation via compute shader (`spbrdf.glsl`), l'écrire dans une texture bidimensionnelle au format `GL_RG16F`, et la lier statiquement à l'unité de texture 2 pour tous les appels de rendu PBR.
[LOCAL:sorting] [ANCRÉ] [ARCHI-REVIEW] Effectuer un tri de type back-to-front (du fond vers l'avant) sur le CPU ou le GPU avant d'appeler le rendu des quads de sphères transparents (Billboard Mode) afin d'assurer l'exactitude du mélange alpha.

---

## 4. CONVENTIONS DE NOMMAGE, TYPES ET BUDGETS MÉTROLOGIQUES

[GLOBAL] [ANCRÉ] [COMPILATEUR] La structure `SphereInstance` doit mesurer exactement 128 octets, être alignée sur 64 octets et respecter le layout mémoire suivant :
  * `mat4 model` : 64 octets (matrice de transformation de l'instance)
  * `vec3 albedo` : 12 octets (couleur diffuse de base)
  * `float metallic` : 4 octets
  * `float roughness` : 4 octets
  * `float ao` : 4 octets (occlusion ambiante par défaut à 1.0)
  * `float padding` : 4 octets
  * `vec3 prev_center` : 12 octets (position du centre de l'instance à la frame précédente)
  * `float padding2` : 20 octets (pour atteindre la taille totale de 128 octets et l'alignement)
[GLOBAL] [MVP/TRANSITION] [RUNTIME/PROFILER] Le budget nominal de mémoire vidéo (VRAM) au repos est borné à 101 MB, réparti comme suit :
  * Textures : ~99 MB (comprenant l'environnement HDR, les cartes de prefilter, d'irradiance, la BRDF LUT, le MRT FBO de scène, la chaîne Bloom, la DoF et la 3D LUT).
  * Tampons (Buffers) : ~40 KB (VBOs/EBOs/SSBOs/UBOs/PBOs).
  * Shaders compilés : ~2 MB.
[LOCAL:postprocess] [ANCRÉ] [COMPILATEUR] Définir les résolutions et caractéristiques métrologiques des structures de post-processing suivantes :
  * FBO Principal : Résolution nominale de 1920x1080.
  * Chaîne de Bloom : 6 niveaux de mipmaps au format `GL_RGBA16F`.
  * Grille de Look-Up Table 3D : Résolution géométrique fixe de 32x32x32 au format `GL_RGB16F` ou `GL_RGBA16F`.
  * Cache LRU de variantes composites de post-processing : Taille maximale fixée à 32 entrées.
[LOCAL:ibl] [ANCRÉ] [COMPILATEUR] Définir les dimensions et formats des tampons de l'IblCoordinator :
  * Texture d'environnement HDR : 2048x1024 au format `GL_RGBA16F`.
  * Carte de Prefilter Spéculaire : 1024x1024 avec 5 niveaux de mipmaps au format `GL_RGBA16F`.
  * Carte d'Irradiance Diffuse : 64x64 au format `GL_RGBA16F`.
  * BRDF LUT : 512x512 au format `GL_RG16F`.
  * Carte de Réduction de Luminance : 64x64 descendant à 1x1 au format `GL_R32F`.
  * Buffer d'analyse de luminance CPU (`lum_histogram_buffer`) : Alloué pour loger exactement 64x64 floats (4096 floats).
[LOCAL:scene] [ANCRÉ] [RUNTIME/PROFILER] La scène nominale est constituée d'une grille 2D de 10 colonnes par 10 lignes totalisant 100 sphères, disposée sur le plan Z = 0 avec un espacement fixe (`spacing`) de 2.5 unités entre les centres, centrée à l'origine (dimensions de la grille : 22.5 x 22.5 unités).
[LOCAL:scene] [ANCRÉ] [COMPILATEUR] La grille tridimensionnelle de sondes de coefficients SH (Spherical Harmonics) est fixée à une résolution géométrique de 21x21x3 voxels distribuée sur 7 textures 3D au format `GL_RGBA16F`.
[LOCAL:camera] [ANCRÉ] [RUNTIME/PROFILER] Les paramètres d'initialisation de l'orbite de la caméra sont : distance à l'origine = 20.0 unités, lacet (yaw) = -90.0°, tangage (pitch) = 0.0°, FOV vertical = 60.0°, et plans de troncature Z = [0.1, 1000.0].
[GLOBAL] [ANCRÉ] [RUNTIME/PROFILER] La durée nominale du fondu de transition (`transition_duration`) lors de l'application d'un nouvel environnement HDR is 250 millisecondes.
[LOCAL:sorting] [ANCRÉ] [COMPILATEUR] Les modes de tri de transparence supportés sont définis par les énumérations : `CPU_QSORT` (tri rapide standard), `CPU_RADIX` (tri par base), et `GPU_BITONIC` (tri bitonique par compute shader).
