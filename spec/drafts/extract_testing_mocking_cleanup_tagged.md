# Audit de l'Architecture de Test, de la Qualité et du Nettoyage C11

## 1. INVARIANTS DU TDD ET ARCHITECTURE DES MOCKS C11
- [LOCAL:mocking] [ANCRÉ] [COMPILATEUR] Redéfinir à la compilation toutes les fonctions externes (OpenGL, GLAD, Log) dépendantes du code testé directement dans le fichier de test standalone.
- [LOCAL:testing] [ANCRÉ] [GREP] Inclure directement le fichier source `.c` cible dans le fichier de test immédiatement après la définition des stubs et des mocks pour tester les fonctions statiques.
- [LOCAL:testing] [ANCRÉ] [CTEST/ASAN] Exécuter les tests unitaires standalone sous forme de binaires en C pur exempts de toute initialisation OpenGL, pilote graphique ou contexte matériel.
- [LOCAL:mocking] [ANCRÉ] [GREP] Privilégier le masquage de symboles à la compilation à l'usage de frameworks de mocking dynamiques ou d'injections de dépendances à l'exécution.
- [LOCAL:mocking] [MVP/TRANSITION] [GREP] Synchroniser manuellement les fonctions mockées en cas de modification des signatures de l'API réelle.

## 2. PROTOCOLES DE TESTS VISUELS ET NON-RÉGRESSION GRAPHIQUE
- [LOCAL:visual_test] [ANCRÉ] [GREP] Nommer les fichiers de référence selon la nomenclature stricte `ref_<view>_<mode>_<effect>.png`.
- [LOCAL:visual_test] [ANCRÉ] [CTEST/ASAN] Activer le mode atténué (`PRESET_SUBTLE`) pour les tests de régression visuelle des effets de post-traitement afin de limiter la surcharge des cartes de différence.
- [LOCAL:visual_test] [ANCRÉ] [CTEST/ASAN] Exécuter le test de flou de mouvement (motion blur) en séquence double-frame (Frame N-1 statique pour initialiser `previousViewProj`, Frame N avec rotation déterministe).
- [LOCAL:visual_test] [ANCRÉ] [GREP] Restreindre le mouvement de caméra à la rotation pure (yaw/pitch) et exclure toute translation lors des séquences de test de flou de mouvement pour éviter les deltas de parallaxe.
- [LOCAL:visual_test] [ANCRÉ] [GREP] Fixer la distance de la caméra à `25.0` et cibler l'origine pour toutes les captures de référence de géométries standardisées.
- [LOCAL:visual_test] [ANCRÉ] [GREP] Imposer l'usage de billboards avec imposteurs raytracés pour le rendu des primitives sphériques de test.
- [LOCAL:visual_test] [ANCRÉ] [GREP] Exclure les étapes de génération géométrique côté CPU (ex: `icosphere_generate`) pendant les phases de tests visuels.
- [GLOBAL] [ANCRÉ] [COMPILATEUR] Imposer l'écrêtage analytique de la rugosité à `MIN_ROUGHNESS = 0.03` à la place de l'utilisation de `fwidth` pour garantir la portabilité inter-constructeurs.
- [LOCAL:visual_test] [ANCRÉ] [RENDERDOC/VISUAL] Multiplier par 5 l'intensité des cartes de différence générées automatiquement lors des divergences détectées en CI.

## 3. RÈGLES DE RÉSILIENCE ET NETTOYAGE MÉMOIRE (RAII / OPENGL CLEANUP)
- [LOCAL:cleanup] [ANCRÉ] [COMPILATEUR] Encapsuler le suivi et la libération automatique des ressources dans des blocs de portée utilisant `__attribute__((cleanup))` via les macros `HYBRID_FUNC_TIMER` et `GPU_STAGE_PROFILER`.
- [GLOBAL] [ANCRÉ] [CTEST/ASAN] Proscrire la copie superficielle de structures gérant des allocations dynamiques et imposer le passage par pointeur ou l'usage de `TRANSFER_OWNERSHIP`.
- [GLOBAL] [ANCRÉ] [COMPILATEUR] Déclarer les variables locales gérées par RAII dans l'ordre inverse de leur destruction attendue (LIFO).
- [GLOBAL] [ANCRÉ] [GREP] Placer les macros de contournement `RAII_SATISFY_FILE` et `RAII_SATISFY_FREE` sur les chemins de retour précoce pour résoudre les faux positifs de Clang-Tidy.
- [LOCAL:opengl] [ANCRÉ] [CTEST/ASAN] Calculer dynamiquement et allouer le nombre exact de niveaux de mipmaps lors de la création de textures HDR avant d'invoquer `glTexStorage2D`.
- [LOCAL:opengl] [ANCRÉ] [CTEST/ASAN] Appeler `glObjectLabel` uniquement après la première liaison (`glBind`) effective de la ressource OpenGL (VAO, VBO, texture).
- [LOCAL:opengl] [ANCRÉ] [CTEST/ASAN] Verrouiller l'accès aux textures globales par défaut (`dummy_black_tex`) pour empêcher leur destruction fortuite lors de la libération des framebuffers locaux.
- [LOCAL:opengl] [ANCRÉ] [CTEST/ASAN] Utiliser des SSBO persistants configurés avec `GL_CLIENT_STORAGE_BIT` à l'exclusion de lectures synchrones via PBO pour les calculs de luminance.
- [LOCAL:opengl] [ANCRÉ] [CTEST/ASAN] Réassocier toutes les unités de texture utilisées (0 à 6) à une texture par défaut valide à la fin de `postprocess_resize` pour éviter les avertissements de base level non défini.
- [LOCAL:opengl] [ANCRÉ] [CTEST/ASAN] Uniformiser les signatures d'attributs de sommets des shaders et désactiver/réinitialiser les diviseurs des slots de VAO 8-15 pour prévenir les recompilations de shader à l'exécution.
- [GLOBAL] [ANCRÉ] [CTEST/ASAN] Libérer systématiquement les ressources des systèmes dynamiques (ex: instanciation N-body) avant d'exécuter une ré-initialisation.
- [GLOBAL] [ANCRÉ] [COMPILATEUR] Implémenter une routine explicite de déplacement/échange de pointeurs (ex: `gpu_stage_move`) pour les structures complexes au lieu de l'affectation par valeur `=`.
- [GLOBAL] [MVP/TRANSITION] [GREP] Documenter systématiquement la sémantique de propriété (propriétaire vs référence) de chaque pointeur au sein des définitions de structures.

## 4. INTERDICTIONS FORMELLES EN QUALITÉ ET TESTING (ANTI-PATTERNS)
- [LOCAL:testing] [ANTI-PATTERN] [RENDERDOC/VISUAL] Valider un correctif de rendu graphique ou de post-traitement par de simples traces écrites dans la console au lieu de tests de régression visuelle.
- [GLOBAL] [ANTI-PATTERN] [CTEST/ASAN] Réaliser des affectations par valeur ou des copies superficielles (`*dest = *src`) de structures détenant des pointeurs vers de la mémoire dynamique.
- [LOCAL:opengl] [ANTI-PATTERN] [CTEST/ASAN] Appeler la fonction `glObjectLabel` sur un identifiant d'objet non lié (unbound) au moins une fois au préalable.
- [LOCAL:opengl] [ANTI-PATTERN] [CTEST/ASAN] Détruire ou altérer des ressources et textures partagées à l'échelle globale lors de la destruction d'objets locaux.
- [LOCAL:opengl] [ANTI-PATTERN] [CTEST/ASAN] Effectuer des lectures de buffers OpenGL synchrones bloquant le CPU sur le thread principal de rendu.
- [LOCAL:opengl] [ANTI-PATTERN] [CTEST/ASAN] Laisser des unités de texture non liées ou à 0 entre les frames de rendu.
- [LOCAL:opengl] [ANTI-PATTERN] [CTEST/ASAN] Introduire des différences de mise en page des sommets (vertex format) entre les shaders de géométrie et de billboard provoquant des recompilations à la volée.
- [LOCAL:mocking] [ANTI-PATTERN] [GREP] Utiliser des bibliothèques de mock dynamiques ou des mécanismes d'injection de dépendances complexes pour le test unitaire bas niveau.
- [GLOBAL] [ANTI-PATTERN] [GREP] Insérer des directives de désactivation de l'analyse statique `// NOLINT` pour masquer des fuites présumées au lieu d'employer des indicateurs chirurgicaux `RAII_SATISFY_*`.
- [GLOBAL] [ANTI-PATTERN] [GREP] Déclarer des pointeurs de chaînes dynamiques (`char*`) au sein de structures persistantes lorsqu'un tableau fixe de caractères (`char name[N]`) est possible.
- [LOCAL:visual_test] [ANTI-PATTERN] [CTEST/ASAN] Utiliser des translations spatiales de caméra dans les scénarios de test visuel du flou de mouvement.
