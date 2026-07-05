# RAPPORT D'AUDIT ARCHITECTURAL BIDIRECTIONNEL - V1

Ce rapport confronte les règles de la [Constitution](file:///home/latty/Prog/__PERSO__/suckless-ogl/spec/00_constitution.md) à la réalité de l'implémentation des modules [app.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/app.c), [scene.h](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/scene.h) (et ses fichiers associés d'initialisation, rendu et nettoyage), ainsi que [ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c).

---

### 1. VIOLATIONS DU CODE ([ANCRÉ] → Le code est fautif)

Voici la liste des endroits exacts où le code source actuel enfreint des règles [ANCRÉ] de la Constitution :

#### A. Gestion des erreurs et point de sortie unique (Ressources non libérées)
* **Règle enfreinte** : Domaine I - Règle 10 (« Toute fonction acquérant plusieurs ressources doit utiliser un point de sortie unique marqué par l'étiquette 'cleanup' et atteint via 'goto cleanup'. ») et Règle 11 (« Interdiction de libérer des ressources par des appels free() successifs en cascade le long des chemins d'erreur. »).
* **Emplacement** : [scene_init.c:L427-503](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427-L503) (dans la fonction [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427-L503)).
* **Description** : La fonction [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427-L503) alloue 4 structures opaques (`gpu`, `shaders`, `simulation`, `visuals`) et de nombreuses ressources OpenGL (shaders, buffers, VAO). En cas d'échec d'une étape (ex: échec de chargement des shaders), elle effectue des retours anticipés (`return 0`) sans effectuer de nettoyage local ou rediriger vers un label `cleanup`.
* **Conséquences** : Bien que l'appelant (`scene_subsys_init`) appelle ensuite [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82-L134) en cas d'erreur, cette dernière n'est **pas sécurisée** contre les pointeurs partiels. Par exemple, si l'allocation de `scene->visuals` échoue mais que `scene->gpu` est valide, [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82-L134) va dereferencer et détruire les sous-composants. Mais si `scene->gpu` ou `scene->shaders` n'avait pas pu être alloué, [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82-L134) provoquerait un plantage (Ségfault) car elle tente de déréférencer les pointeurs de sous-structures (ex: `scene->shaders->pbr_instanced` à [scene_cleanup.c:L24](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L24)) sans garde contre `NULL`.

#### B. Configuration non systématique de la compatibilité ascendante OpenGL (Forward Compat)
* **Règle enfreinte** : Domaine II - Règle 26 (« avec configuration systématique du flag GLFW_OPENGL_FORWARD_COMPAT lors de l'initialisation du contexte GLFW. »).
* **Emplacement** : [window.c:L26-28](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/window.c#L26-L28) (dans la fonction [window_create](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/window.c#L10-L63)).
* **Description** : L'activation de `GLFW_OPENGL_FORWARD_COMPAT` est actuellement restreinte à macOS par une directive de précompilation :
  ```c
  #ifdef __APPLE__
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  #endif
  ```
  La Constitution exige que ce flag soit activé de façon systématique sur toutes les plateformes.

#### C. Mode Debug d'OpenGL désactivé en Release
* **Règle enfreinte** : Domaine II - Règle 27 (« Initialiser GLFW avec les caractéristiques suivantes : OpenGL 4.4 Core Profile, Debug Context actif (GL_OPENGL_DEBUG_CONTEXT), ... »).
* **Emplacement** : [window.c:L21-25](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/window.c#L21-L25) (dans la fonction [window_create](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/window.c#L10-L63)).
* **Description** : Le contexte de débogage n'est activé que si la macro `NDEBUG` n'est pas définie :
  ```c
  #ifndef NDEBUG
      glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  #else
      glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_FALSE);
  #endif
  ```
  Cela contredit la règle [ANCRÉ] qui impose son activation globale, sans restriction d'environnement de build.

#### D. Barrière mémoire inaccessible / Code mort dans la machine d'état IBL
* **Règle enfreinte** : Domaine III - Règle 67 (« Appeler un unique glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) à l'état final IBL_STATE_DONE ; interdiction d'insérer des barrières à la fin de chaque tranche. »).
* **Emplacement** : [ibl_coordinator.c:L379-381](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L379-L381) et [L406-410](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L406-L410) (dans la fonction [ibl_coordinator_update](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L377-L417)).
* **Description** : La fonction [ibl_coordinator_update](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L377-L417) commence par un retour anticipé si l'état courant est `IBL_STATE_DONE` :
  ```c
  if (coord->state == IBL_STATE_IDLE || coord->state == IBL_STATE_DONE) {
      return coord->state;
  }
  ```
  Par conséquent, le bloc `case IBL_STATE_DONE` situé plus bas dans le `switch` :
  ```c
  case IBL_STATE_DONE:
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      break;
  ```
  est du **code mort** qui n'est jamais atteint. La barrière mémoire n'est donc jamais appelée à la fin de la génération progressive de l'IBL, ce qui crée un risque de corruption de cache de texture lors de l'affichage initial.

#### E. Couplage d'inclusions (Include fan-out) trop élevé dans scene.h
* **Règle enfreinte** : Domaine IV - Règle 73 (« Include fan-out maximum : interdiction de concevoir des en-têtes avec plus de 5 inclusions d'en-têtes de projet. »).
* **Emplacement** : [scene.h:L4-9](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/scene.h#L4-L9) et [L116](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/scene.h#L116).
* **Description** : L'en-tête [scene.h](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/scene.h) inclut les en-têtes de projet suivants :
  1. `app_settings.h`
  2. `billboard_renderer.h`
  3. `icosphere.h`
  4. `instanced_rendering.h`
  5. `scene_config.h`
  6. `scene_lighting.h`
  7. `ssbo_rendering.h` (si `USE_SSBO_RENDERING` est défini)
  8. `app_subsystem.h`
  Soit un total de 8 inclusions de projet, ce qui dépasse largement la limite autorisée de 5.

#### F. Déclaration incomplète de l'alignement et layout de SphereInstance
* **Règle enfreinte** : Domaine IV - Règle 80 (« respecter son layout strict (mat4 model, vec3 albedo, float metallic, float roughness, float ao, float padding, vec3 prev_center, float padding2[5]). »).
* **Emplacement** : [sphere_types.h:L25-34](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/sphere_types.h#L25-L34).
* **Description** : Le type [SphereInstance](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/sphere_types.h#L25-L34) ne définit pas explicitement le tableau `float padding2[5]` à la fin de sa structure. Il s'arrête à `vec3 prev_center`. Bien que la structure mesure effectivement 128 octets en mémoire à cause de l'attribut d'alignement `__attribute__((aligned(64)))` qui pousse le compilateur à ajouter du padding implicite, la définition formelle de la structure contrevient au layout strict exigé par la Constitution.

#### G. Mauvais raccourci clavier pour le mode Fil de fer (Wireframe)
* **Règle enfreinte** : Domaine IV - Règle 83 (« Raccourcis clavier obligatoires : ... W (fil de fer) ... »).
* **Emplacement** : [app_input.c:L432](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/app_input.c#L432) (dans la fonction [handle_system_key_input](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/app_input.c#L424-L467)).
* **Description** : Le mode fil de fer est associé à la touche `Z` (`case GLFW_KEY_Z`) au lieu de la touche `W` (`GLFW_KEY_W`). Aucun traitement de la touche `W` n'existe dans le gestionnaire d'entrées.

---

### 2. DETTE TECHNIQUE ET TRANSITION ([MVP/TRANSITION] → Cibles valides)

Voici les éléments qui sont des violations tolérées sous forme de transition ou dette technique en cours :

#### A. Dépassement de la limite de taille des modules (500 LOC)
* **Règle ciblée** : Domaine IV - Règle 71 (« Limite de 500 lignes de code (LOC) par module ; un module dépassant 500 LOC doit être décomposé en unités spécialisées. »).
* **Emplacement et Mesure** :
  - **Module Scene** : composé de [scene_init.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c) (535 LOC), [scene_render.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_render.c) (379 LOC), [scene_cleanup.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c) (135 LOC), [scene_nbody.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_nbody.c) (143 LOC) et [scene.h](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/scene.h) (124 LOC). Le total cumulé dépasse les 1300 LOC. Même pris individuellement, le fichier d'initialisation [scene_init.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c) outrepasse à lui seul le seuil avec ses 535 lignes.
  - **Module IblCoordinator** : composé de [ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c) (450 LOC) et [ibl_coordinator.h](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/ibl_coordinator.h) (132 LOC), soit un total de 582 LOC.
  - **Module App** : composé de [app.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/app.c) (413 LOC) et [app.h](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/app.h) (87 LOC), soit pile 500 LOC. Il est à la limite absolue de la tolérance.

#### B. Utilisation de pthread.h en dehors de la PAL (Migration incomplète)
* **Règle ciblée** : Domaine I - Règle 6 (« Interdiction d'inclure des en-têtes système natifs ... dans les modules de logique métier, de scène et de rendu hors de l'unité PAL ») et Domaine II - Règle 45 (« La gestion des threads dans async_loader et light_probes est en cours de migration depuis <pthread.h> direct vers l'abstraction PAL native. »).
* **Emplacement** : [light_probes.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/light_probes.c) (lighting de Scene) et [async_loader.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/async_loader.c) (dépendance directe de App).
* **Description** : Les deux fichiers incluent et utilisent directement `<pthread.h>` ainsi que ses primitives (`pthread_mutex_t`, `pthread_cond_t`, `pthread_create`, `pthread_join`) au lieu de passer par des abstractions de la PAL. C'est une dette de transition explicitement reconnue par la Règle 45.

#### C. Budget nominal de VRAM (101 Mo) au repos
* **Règle ciblée** : Domaine IV - Règle 77 (« Le budget nominal de mémoire vidéo (VRAM) au repos est borné à 101 Mo... »).
* **Description** : Ce budget est une cible globale qui nécessite un suivi continu par profilage GPU.

---

### 3. FAILLE OU HALLUCINATION DE LA CONSTITUTION (La Constitution a tort)

Voici les incohérences techniques relevées dans la Constitution, où les exigences formelles entrent en conflit direct avec la réalité d'un code propre et moderne :

#### A. Boucle fermée de la machine d'états d'IblCoordinator
* **Problème** : La règle 62 impose un cycle d'états strictement linéaire :
  `IBL_STATE_IDLE -> IBL_STATE_LUMINANCE -> IBL_STATE_LUMINANCE_WAIT -> IBL_STATE_SPECULAR_INIT -> IBL_STATE_SPECULAR_MIPS -> IBL_STATE_IRRADIANCE -> IBL_STATE_DONE.`
* **Réalité** : Pour permettre des rechargements consécutifs de cartes d'environnement (chargement de nouvelles HDR en cours d'exécution), la machine d'état doit pouvoir boucler. L'implémentation dans [ibl_coordinator.c:L446](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L446) réinitialise l'état à `IBL_STATE_IDLE` une fois les textures récupérées, et [ibl_coordinator_start](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L182-L192) fait repartir la transition de `IBL_STATE_IDLE` vers `IBL_STATE_LUMINANCE`. La linéarité stricte sans rebouclage est une hallucination théorique qui empêcherait le changement dynamique d'environnement.
* **Correction exacte à apporter dans [00_constitution.md](file:///home/latty/Prog/__PERSO__/suckless-ogl/spec/00_constitution.md)** :
  ```diff
  -* [LOCAL:ibl][ANCRÉ][RUNTIME/PROFILER] Cycle d'états de l'IblCoordinator : transition stricte IBL_STATE_IDLE -> IBL_STATE_LUMINANCE -> IBL_STATE_LUMINANCE_WAIT -> IBL_STATE_SPECULAR_INIT -> IBL_STATE_SPECULAR_MIPS -> IBL_STATE_IRRADIANCE -> IBL_STATE_DONE.
  +* [LOCAL:ibl][ANCRÉ][RUNTIME/PROFILER] Cycle d'états de l'IblCoordinator : transition stricte IBL_STATE_IDLE -> IBL_STATE_LUMINANCE -> IBL_STATE_LUMINANCE_WAIT -> IBL_STATE_SPECULAR_INIT -> IBL_STATE_SPECULAR_MIPS -> IBL_STATE_IRRADIANCE -> IBL_STATE_DONE -> IBL_STATE_IDLE (après consommation des résultats).
  ```

#### B. Slicing temporel déphasé pour les Mips Specular
* **Problème** : La règle 63 exige : `... Specular Mip 0 en 24 tranches, Mip 1 en 8 tranches, Mips 2-4 sur 1 frame (tail grouping) ...`.
* **Réalité** : Dans [ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c), le regroupement commence à partir du niveau de Mip 3 (`IBL_SPECULAR_MIP_GROUPING_START_MIP = 3`), et non 2. La mip 2 est traitée séparément avec une seule tranche sur sa propre frame (`total_slices = 1`). La phrase « Mips 2-4 sur 1 frame » est contredite par la logique d'optimisation réelle du code qui commence le tail grouping au niveau 3+.
* **Correction exacte à apporter dans [00_constitution.md](file:///home/latty/Prog/__PERSO__/suckless-ogl/spec/00_constitution.md)** :
  ```diff
  -* [LOCAL:ibl][ANCRÉ][RUNTIME/PROFILER] Slicing temporel IBL : sur GPU matériel, Specular Mip 0 en 24 tranches, Mip 1 en 8 tranches, Mips 2-4 sur 1 frame (tail grouping), Irradiance en 12 tranches, Luminance en 2 dispatches. Limite de 10-15 ms par tranche.
  +* [LOCAL:ibl][ANCRÉ][RUNTIME/PROFILER] Slicing temporel IBL : sur GPU matériel, Specular Mip 0 en 24 tranches, Mip 1 en 8 tranches, Mip 2 sur 1 frame, Mips 3+ sur 1 frame (tail grouping), Irradiance en 12 tranches, Luminance en 2 dispatches. Limite de 10-15 ms par tranche.
  ```

#### C. Incohérence mathématique du layout de SphereInstance
* **Problème** : La règle 80 impose que [SphereInstance](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/sphere_types.h#L25-L34) ait le layout suivant :
  `(mat4 model, vec3 albedo, float metallic, float roughness, float ao, float padding, vec3 prev_center, float padding2[5])`.
* **Réalité** : Calculons l'empreinte mémoire exacte de ces champs :
  - `mat4 model` : 16 floats * 4 octets = 64 octets
  - `vec3 albedo` : 3 floats * 4 octets = 12 octets
  - `float metallic`, `float roughness`, `float ao`, `float padding` : 4 floats * 4 octets = 16 octets
  - `vec3 prev_center` : 3 floats * 4 octets = 12 octets
  Sous-total = 104 octets.
  Si l'on ajoute `float padding2[5]` (5 floats * 4 octets = 20 octets), le total des champs déclarés est de 124 octets.
  Or, la structure étant alignée sur 64 octets (`__attribute__((aligned(64)))`), le compilateur force sa taille à un multiple de 64, soit **128 octets**. Il insère donc de façon invisible 4 octets de padding de fin (124 + 4 = 128).
  Pour que la structure fasse *exactement* 128 octets de champs déclarés (sans padding résiduel invisible imposé par l'alignement), il faudrait déclarer `float padding2[6]` (24 octets, 104 + 24 = 128). L'indication `padding2[5]` dans la Constitution est donc une erreur mathématique de calcul de layout. De plus, il est plus propre en C de laisser le compilateur gérer l'alignement à 128 octets automatiquement via l'attribut d'alignement.
* **Correction exacte à apporter dans [00_constitution.md](file:///home/latty/Prog/__PERSO__/suckless-ogl/spec/00_constitution.md)** :
  ```diff
  -* [GLOBAL][ANCRÉ][COMPILATEUR] La structure SphereInstance doit mesurer exactement 128 octets, être alignée sur 64 octets, et respecter son layout strict (mat4 model, vec3 albedo, float metallic, float roughness, float ao, float padding, vec3 prev_center, float padding2[5]).
  +* [GLOBAL][ANCRÉ][COMPILATEUR] La structure SphereInstance doit mesurer exactement 128 octets, être alignée sur 64 octets, et respecter son layout strict (mat4 model, vec3 albedo, float metallic, float roughness, float ao, float padding, vec3 prev_center, et alignement automatique à 128 octets par le compilateur).
  ```
