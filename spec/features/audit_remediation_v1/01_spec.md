# SPÉCIFICATION FONCTIONNELLE : RÉPARATION DE L'AUDIT ARCHITECTURAL (LOT V1)

Ce document spécifie le comportement attendu et les exigences fonctionnelles pour la correction du premier lot d'anomalies identifiées lors de l'audit d'architecture. Conformément aux principes du développement orienté spécifications (Spec-Driven Development), ce document définit uniquement le **QUOI** et le **POURQUOI**, sans intégrer de code source d'implémentation C11.

---

## 1. Objectif et Périmètre

L'objectif de ce lot de remédiations est d'éradiquer deux failles architecturales majeures touchant la stabilité et la cohérence de l'affichage dans le moteur [suckless-ogl](file:///home/latty/Prog/__PERSO__/suckless-ogl) :

1. **Gestion robuste de l'initialisation partielle et du nettoyage de la scène** :
   - **Composants concernés** : [scene_init.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c) et [scene_cleanup.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c).
   - **Problématique** : Lors de l'initialisation de la scène dans [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427), de nombreuses ressources système (structures opaques sur le tas pour `gpu`, `shaders`, `simulation`, `visuals`) et ressources OpenGL (VAO, VBO, shaders compilés) sont allouées. Si l'un des sous-systèmes échoue à s'initialiser, la fonction retourne immédiatement `0`. L'appelant invoque alors [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82) pour libérer les ressources. Cependant, cette fonction de nettoyage n'a pas été conçue pour tolérer des structures partiellement allouées (absence de vérification des pointeurs à `NULL`), ce qui provoque des plantages (déréférencement de pointeurs nuls / segfaults) et laisse des ressources orphelines (fuites de mémoire RAM/VRAM).
   - **Périmètre de correction** : Rendre [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) conforme aux exigences de sortie unique de la Constitution et immuniser [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82) contre les structures non initialisées ou partielles.

2. **Éradication du code mort et garantie de barrière mémoire IBL** :
   - **Composant concerné** : [ibl_coordinator.c](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c).
   - **Problématique** : La fonction de mise à jour [ibl_coordinator_update](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L377) possède un filtre de garde en début de fonction qui retourne immédiatement si l'état courant de l'IBL est `IBL_STATE_DONE` (ou `IBL_STATE_IDLE`). Le bloc `case IBL_STATE_DONE` situé dans le `switch` principal, contenant l'appel à la barrière mémoire OpenGL (`glMemoryBarrier`), n'est donc jamais exécuté. Ce code mort empêche la synchronisation GPU requise pour garantir que les écritures de textures par les compute shaders IBL sont visibles et cohérentes pour le pipeline de rendu principal.
   - **Périmètre de correction** : Garantir l'invocation déterministe et unique de la barrière mémoire lors du passage à l'état finalisé de l'IBL.

---

## 2. Exigences Fonctionnelles et de Résilience

### A. Gestion robuste et transactionnelle de la scène

* **Principe de Transactionnalité** :
  L'initialisation de la scène doit se comporter comme une transaction atomique. Si une étape quelconque de [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) échoue (allocation mémoire ou initialisation de ressource OpenGL/système), l'intégralité des ressources allouées *au sein de cet appel* doit être libérée immédiatement. L'état du système doit être restauré à l'identique de l'instant précédant l'appel.
* **Point de sortie unique** :
  Conformément aux règles du Domaine I de la Constitution ([Constitution : Domaine I, Règle 10 et 11](file:///home/latty/Prog/__PERSO__/suckless-ogl/spec/00_constitution.md#L10-L11)), [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) doit centraliser toute sa logique de libération d'erreur dans un unique bloc `cleanup` situé en fin de fonction. L'utilisation de retours anticipés (`return 0`) après avoir commencé à acquérir des ressources est interdite. Les sauts vers ce bloc se feront exclusivement par l'instruction `goto cleanup`.
* **Immunisation contre les pointeurs partiels** :
  La fonction globale de nettoyage [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82) doit être intégralement sécurisée par des vérifications systématiques contre les pointeurs nuls. Avant d'accéder à un membre d'une structure opaque (`scene->visuals`, `scene->gpu`, `scene->shaders`, etc.) ou d'appeler une routine de destruction, le moteur doit s'assurer que le conteneur parent est valide.
* **Idempotence de nettoyage** :
  L'invocation successive de [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82) sur une même structure (ou sur une structure dont les sous-champs sont déjà à `NULL` ou à `0`) ne doit provoquer aucune erreur, double libération, ou crash.

### B. Garantie de synchronisation de la barrière mémoire IBL

* **Exécution déterministe de la barrière** :
  La barrière mémoire OpenGL (`glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)`) doit être exécutée exactement une fois à la fin de la génération de l'IBL.
* **Moment de la transition** :
  L'appel à la barrière doit avoir lieu immédiatement lors de la transition d'état depuis `IBL_STATE_IRRADIANCE` (lorsque la dernière tranche est terminée) vers `IBL_STATE_DONE`.
* **Résolution du court-circuit** :
  L'architecture de validation de la machine d'états dans [ibl_coordinator_update](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/ibl_coordinator.c#L377) ne doit plus bloquer le traitement de l'état final. Le code gérant la barrière OpenGL ne doit plus être considéré comme mort ou inaccessible.

---

## 3. Critères d'Acceptation (Testabilité)

### A. Non-régression et stabilité mémoire (Scene)

1. **Injection de pannes (Fault Injection)** :
   - Nous simulerons des échecs d'initialisation à chaque étape de [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) (ex: en forçant des échecs d'allocation de `scene->visuals`, des échecs de compilation de shaders via de mauvais chemins d'accès ou des mocks d'allocateur).
   - **Résultat attendu** : Aucun crash (segfault) ne doit se produire et le programme appelant doit se terminer ou propager l'erreur proprement.
2. **Validation par analyseurs dynamiques (Valgrind / ASan)** :
   - L'ensemble des scénarios de test d'erreur d'initialisation de la scène doit être exécuté sous Valgrind (outil Memcheck) et avec l'AddressSanitizer actif.
   - **Résultat attendu** : ZÉRO octet de fuite de mémoire système (RAM) et ZÉRO accès invalide en écriture ou en lecture (out-of-bounds, use-after-free).
3. **Audit des ressources GPU** :
   - L'exécution de la suite de tests avec défaillance simulée doit être validée via OpenGL Debug Output ou un outil de diagnostic graphique.
   - **Résultat attendu** : Toutes les ressources OpenGL créées avant l'échec (VAO, VBO, shaders, etc.) doivent être correctement libérées. Aucune fuite de VRAM ne doit subsister sur le GPU.

### B. Fiabilité de la synchronisation GPU (IBL)

1. **Validation du Call Graph et Traces** :
   - Une trace de diagnostic ou une vérification dans le test unitaire doit confirmer que la fonction `glMemoryBarrier` est appelée une et une seule fois à la fin du processus de génération IBL (lors de la bascule vers `IBL_STATE_DONE`).
2. **Contrôle de l'absence de code mort** :
   - L'analyse statique du code (ou l'exécution avec couverture de code comme Gcov) doit démontrer que le bloc de traitement de la barrière mémoire est couvert à 100% lors d'une génération IBL complète.
3. **Intégrité visuelle du rendu** :
   - Lors de la première frame d'affichage après la finalisation de l'IBL, les textures d'irradiance et de réflexion spéculaire doivent être immédiatement exploitables par les shaders de rendu sans scintillement (flickering), textures noires temporaires ou artefacts visuels dus à un manque de synchronisation de cache GPU.

---

## 4. Cas Limites (Edge Cases) et Risques de Régression

* **Ordre de destruction des dépendances OpenGL** :
  Lors du nettoyage de la scène, la libération des buffers (VBO, VAO) et des textures doit se faire avant la suppression ou le détachement des contextes et des shaders associés. Toute inversion pourrait amener le driver OpenGL à tenter de manipuler des objets orphelins.
* **Risque de double libération (Double Free)** :
  Si [scene_init](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_init.c#L427) libère ses ressources internes localement via son étiquette `cleanup` puis renvoie `0`, l'appelant (`scene_subsys_init`) pourrait invoquer à nouveau [scene_cleanup](file:///home/latty/Prog/__PERSO__/suckless-ogl/src/scene_cleanup.c#L82). Pour écarter ce risque, chaque ressource libérée dans le bloc `cleanup` local de `scene_init` doit être explicitement remise à sa valeur par défaut (`NULL` pour les pointeurs, `0` pour les poignées/handles OpenGL).
* **Réinitialisation de la machine d'état IBL** :
  L'IblCoordinator supporte le rechargement à chaud en repassant de l'état `IBL_STATE_DONE` à `IBL_STATE_IDLE`. Lors de ce rebouclage, la réinitialisation des variables d'état ne doit pas déclencher la barrière de manière intempestive ni laisser subsister de verrouillage ou de textures dans un état incohérent.
* **Impact sur le budget de temps de trame (Frame Budget)** :
  L'appel à `glMemoryBarrier` est une opération bloquante pour la mémoire GPU. S'assurer que son appel unique ne génère pas de micro-stuttering (saccade) perceptible sur le thread de rendu principal. Son coût doit rester confiné dans le budget nominal de la frame de transition.
