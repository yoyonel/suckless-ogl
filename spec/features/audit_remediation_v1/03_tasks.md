# Checklist de Développement : Remédiation de l'Audit Architectural (Lot V1)

Ce document fournit la liste des tâches de développement unitaires (estimées à 1-2 heures chacune) nécessaires pour implémenter la remédiation de l'audit architectural dans le moteur **suckless-ogl**.

---

## Phase 1 : Modifications Structurelles et En-têtes

### [x] Tâche 1.1 : Déclaration du drapeau de barrière dans IBLCoordinator
* **Fichier cible** : [ibl_coordinator.h](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/ibl_coordinator.h#L76)
* **Description** : Ajouter le membre `bool barrier_executed;` à la fin de la structure [IBLCoordinator](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/ibl_coordinator.h#L76) pour suivre si la barrière mémoire GPU a été émise lors de l'état `DONE`. S'assurer de la présence de l'inclusion `<stdbool.h>` dans le fichier d'en-tête.
* **Validation** :
  - Exécuter la compilation du projet (ex: `cmake --build build` ou script équivalent) pour vérifier l'absence d'erreurs de syntaxe ou d'alignement de structure.

### [x] Tâche 1.2 : Initialisation du drapeau dans le constructeur de l'IBL
* **Fichier cible** : [ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L134)
* **Description** : Dans la fonction [ibl_coordinator_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L134), initialiser explicitement le membre `coord->barrier_executed` à `false` lors de l'initialisation de l'instance par affectation de structure vide.
* **Validation** :
  - Compilation réussie sans warning.

### [x] Tâche 1.3 : Réinitialisation du drapeau lors du reset de l'IBL
* **Fichier cible** : [ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L162)
* **Description** : Dans la fonction [ibl_coordinator_reset](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L162), remettre `coord->barrier_executed` à `false` pour garantir que tout nouveau cycle de génération d'IBL pourra émettre sa propre barrière mémoire.
* **Validation** :
  - Compilation réussie.

---

## Phase 2 : Restructuration du Flux de Contrôle de l'IBL

### [x] Tâche 2.1 (TDD) : Écrire le test unitaire automatisé tests/test_ibl_coordinator.c et l'enregistrer dans CMakeLists.txt
* **Fichier cible** : [tests/test_ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/tests/test_ibl_coordinator.c)
* **Description** : Écrire le test unitaire automatisé [tests/test_ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/tests/test_ibl_coordinator.c) et l'enregistrer dans [tests/CMakeLists.txt](file:///home/latty/Prog/__PERSO__/suckless-ogl/tests/CMakeLists.txt). Ce test doit instancier un [IBLCoordinator](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/ibl_coordinator.h#L76) et vérifier mécaniquement la transition vers `IBL_STATE_DONE` et l'assertion du drapeau `barrier_executed` (test headless sans fenêtre GLFW).
* **Validation** :
  - Le test compile avec succès et s'exécute, échouant (ou étant incomplet) avant l'implémentation de la Phase 2 (RED).

### [x] Tâche 2.2 : Implémentation dans src/ibl_coordinator.c
* **Fichier cible** : [ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L379)
* **Description** : Implémenter la logique et restructurer la fonction [ibl_coordinator_update](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L379) :
  - Modifier la garde d'entrée : remplacer `if (coord->state == IBL_STATE_IDLE || coord->state == IBL_STATE_DONE)` par `if (coord->state == IBL_STATE_IDLE || (coord->state == IBL_STATE_DONE && coord->barrier_executed))`.
  - Dans le bloc `case IBL_STATE_DONE:`, encapsuler l'appel à `glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)` dans un contrôle conditionnel `if (!coord->barrier_executed)` et mettre `coord->barrier_executed` à `true` juste après l'appel.
* **Validation** :
  - Le test `ctest -R test_ibl_coordinator` passe avec succès (GREEN).

---

## Phase 3 : Sécurisation Défensive et Idempotence du Nettoyage

### [ ] Tâche 3.1 (TDD) : Créer tests/test_scene_cleanup_idempotency.c
* **Fichier cible** : [tests/test_scene_cleanup_idempotency.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/tests/test_scene_cleanup_idempotency.c)
* **Description** : Créer le fichier de test unitaire [tests/test_scene_cleanup_idempotency.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/tests/test_scene_cleanup_idempotency.c) et l'enregistrer dans [tests/CMakeLists.txt](file:///home/latty/Prog/__PERSO__/suckless-ogl/tests/CMakeLists.txt). Ce test doit instancier une structure [Scene](file:///home/latty/Prog/__PERSO__/suckless-ogl/include/scene.h#L27) initialisée à `{0}`, appeler la fonction [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82), et vérifier via ctest sous ASan/Valgrind l'absence de Segfault.
* **Validation** :
  - Le test compile avec succès, est enregistré dans CTest, et échoue (RED) avant la sécurisation de [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82).

### [ ] Tâche 3.2 : Sécurisation des routines internes de nettoyage
* **Fichier cible** : [scene_cleanup.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L22)
* **Description** : Modifier toutes les routines de nettoyage interne statiques pour les rendre tolérantes aux pointeurs nuls :
  - [scene_cleanup_pbr_shaders](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L22) : Guarder l'accès à `scene->shaders` et `scene->gpu`.
  - [scene_cleanup_shaders](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L33) : Guarder l'accès à `scene->shaders` et `scene->gpu`.
  - [scene_cleanup_geometry_buffers](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L44) : Guarder l'accès à `scene->gpu`.
  - [scene_cleanup_buffers](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L52) : Guarder l'accès à `scene->gpu`.
  - [scene_cleanup_textures](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L64) : Guarder l'accès à `scene->gpu`.
  - [scene_cleanup_gpu_resources](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L76) : S'assurer que l'appel ne plante pas si les structures internes sont NULL.
* **Validation** :
  - Le test `ctest -R test_scene_cleanup_idempotency` passe avec succès (GREEN) sous ASan/Valgrind.

### [ ] Tâche 3.3 : Restructuration et sécurisation de scene_cleanup de haut niveau
* **Fichier cible** : [scene_cleanup.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82)
* **Description** :
  - Restructurer la fonction globale [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82) pour qu'elle nettoie les ressources dans un ordre strict (du plus dépendant au plus indépendant).
  - Ajouter des conditions `if (scene->visuals)` et `if (scene->gpu)` avant d'exécuter les routines de nettoyage tierces comme `skybox_cleanup` ou `trail_renderer_cleanup`.
  - Libérer explicitement les structures opaques (`scene->gpu`, `scene->shaders`, `scene->simulation`, `scene->visuals`) avec `free()` et mettre systématiquement leurs pointeurs à `NULL`.
* **Validation** :
  - Le test `ctest -R test_scene_cleanup_idempotency` passe avec succès (GREEN) sous ASan/Valgrind.

---

## Phase 4 : Flux Transactionnel d'Initialisation

### [ ] Tâche 4.1 : Transition de scene_init vers le point de sortie unique
* **Fichier cible** : [scene_init.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427)
* **Description** : Réécrire entièrement le flux de contrôle de la fonction [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) :
  - Déclarer une variable `int success = 0;`.
  - Remplacer tous les `return 0` anticipés (après les allocations initiales) par un saut `goto cleanup;`.
  - Ajouter en fin de fonction l'étiquette `cleanup:`.
  - Si `!success`, appeler la fonction sécurisée [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82).
  - Retourner la valeur de `success`.
* **Validation** :
  - Compilation réussie du projet global.

### [ ] Tâche 4.2 : Sécurisation du point d'entrée subsystem scene_subsys_init
* **Fichier cible** : [scene_init.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L508)
* **Description** : Dans [scene_subsys_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L508), s'assurer que si [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) échoue, la structure globale `app->scene` allouée par `platform_aligned_alloc` est proprement désallouée et le pointeur est mis à `NULL`.
* **Validation** :
  - Compilation réussie du projet global et exécution correcte de la suite de tests unitaires.

---

## Phase 5 : Tests de Non-Régression et Performance

### [ ] Tâche 5.1 : Validation de l'idempotence sous ASan et Valgrind
* **Description** :
  - Compiler le moteur avec le support d'AddressSanitizer (ASan) et UndefinedBehaviorSanitizer (UBSan).
  - Exécuter le test unitaire d'idempotence [tests/test_scene_cleanup_idempotency.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/tests/test_scene_cleanup_idempotency.c) sous ASan et Valgrind.
* **Validation** :
  - Le test `ctest -R test_scene_cleanup_idempotency` exécuté sous ASan ou Valgrind remonte 0 fuite mémoire directe ou indirecte (`no memory leaks`) et 0 erreur de comportement indéfini.

### [ ] Tâche 5.2 : Validation des performances en transition (Fondu au noir)
* **Description** :
  - Configurer et lancer le test de transition ou l'application avec le mode `ENV_TRANSITION_BLACK_SCREEN` (géré par [env_manager.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/env_manager.c#L217)).
  - Confirmer via profilage ou par l'état interne du coordinateur qu'aucun appel redondant ou gaspillage de barrière mémoire GPU n'est émis.
* **Validation** :
  - Le profilage ou les assertions vérifient que `glMemoryBarrier` n'est pas appelée à chaque frame pendant le fondu au noir, conservant ainsi le framerate stable et fluide.
