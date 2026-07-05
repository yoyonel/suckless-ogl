## 1. INVARIANTS ARCHITECTURAUX ET FLUX D'EXÉCUTION

* [LOCAL:ubo] [ANCRÉ] [COMPILATEUR] Correspondance binaire stricte : la structure C de configuration de post-process (`PostProcessUBO_Layout`) doit correspondre octet par octet au bloc uniforme GLSL (`PostProcessBlock_Layout`) sous la disposition `std140`.
* [LOCAL:ubo] [ANCRÉ] [COMPILATEUR] Alignement std140 GPU : aligner les scalaires sur 4 octets (N), les vec2 sur 8 octets (2N), les vec3/vec4/mat4 sur 16 octets (4N).
* [LOCAL:ubo] [ANCRÉ] [COMPILATEUR] Alignement par blocs : regrouper et aligner les blocs logiques de données (Vignette, FXAA, 3D LUT, etc.) sur des frontières de 16 octets.
* [LOCAL:postprocess] [ANCRÉ] [COMPILATEUR] Activation des effets : encodage de l'activation des effets de post-traitement au runtime via un bitmask (champ `active_effects` de type `uint32_t`).
* [LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] Cycle d'états de la machine progressive (`IBLCoordinator`) : transition stricte `LUMINANCE` → `LUMINANCE_WAIT` → `SPEC_INIT` → `SPEC_MIPS` → `IRRADIANCE` → `DONE`.
* [LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] Slicing IBL : subdivision des tâches lourdes de génération IBL en tranches temporelles (24 tranches pour Mip 0, 8 pour Mip 1).
* [LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] Détection fallback CPU : bascule automatique vers un mode non découpé (slicing désactivé) lors d'une exécution sur pilote logiciel (ex: llvmpipe).
* [LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] Couplage temporel des transitions d'affichage : blocage de la transition visuelle (Black Screen) tant que la machine d'état IBL n'a pas finalisé son exécution.
* [LOCAL:dependencies] [ANCRÉ] [ARCHI-REVIEW] Opaque callback : le module de rendu bas niveau (`renderer`) doit exécuter le rendu de l'interface utilisateur via un type opaque `RenderUIFn` (`typedef void (*RenderUIFn)(void* user_data)`).
* [GLOBAL] [ANCRÉ] [COMPILATEUR] Spécification minimale graphique : support et exécution exclusifs sous OpenGL 4.4 Core Profile.

## 2. INTERDICTIONS FORMELLES ET ANTI-PATTERNS

* [LOCAL:ubo] [ANTI-PATTERN] [GREP] Interdiction d'envoyer les paramètres de post-traitement via des appels individuels `glUniform*` ou `glUseProgram` répétés par frame.
* [LOCAL:ubo] [ANTI-PATTERN] [COMPILATEUR] Interdiction d'utiliser des tableaux de padding dans le bloc GLSL (ex: `float padding[3]`) sous disposition `std140` pour éviter l'alignement individuel de chaque élément sur 16 octets.
* [LOCAL:ibl] [ANTI-PATTERN] [ARCHI-REVIEW] Interdiction d'utiliser des pointeurs ou handles de textures OpenGL périmés ou de provoquer des double-libérations (double-free) : mise à NULL obligatoire des handles internes à `IBLCoordinator` dès la récupération des résultats.
* [LOCAL:ibl] [ANTI-PATTERN] [RUNTIME/PROFILER] Interdiction d'effectuer des allocations ou libérations de ressources GPU critiques pendant la phase de mise à jour à chaque frame.
* [GLOBAL] [ANTI-PATTERN] [GREP] Interdiction de dépasser un seuil de 150 lignes effectives (LOC) de code dans le corps d'une fonction.
* [GLOBAL] [ANTI-PATTERN] [ARCHI-REVIEW] Interdiction de coupler directement les dépendances de headers (ex: inclusion mutuelle `app.h` 🔁 `renderer.h`) créant des cycles de compilation.
* [LOCAL:structure] [ANTI-PATTERN] [ARCHI-REVIEW] Interdiction d'intégrer la configuration des paramètres d'effets par défaut dans la fonction d'allocation des ressources graphiques (`postprocess_init`).
* [LOCAL:structure] [ANTI-PATTERN] [ARCHI-REVIEW] Interdiction d'intégrer la logique d'orchestration générale, les calculs physiques ou le dessin direct des géométries au sein d'une seule routine monolithique.

## 3. PATTERNS OBLIGATOIRES ET GESTION DES RESSOURCES

* [LOCAL:ubo] [ANCRÉ] [RUNTIME/PROFILER] Mise à jour UBO : transfert unique par frame de l'intégralité du bloc via `glBufferSubData` en passant une copie locale de la structure C allouée sur la pile (stack).
* [LOCAL:ubo] [ANCRÉ] [COMPILATEUR] Padding individuel GLSL : déclarer les champs de padding sous forme de scalaires individuels (ex: `float _pad1_0; float _pad1_1;`) pour correspondre exactement aux tableaux C.
* [LOCAL:postprocess] [ANCRÉ] [COMPILATEUR] Macro d'évaluation shader : tester l'activation d'un effet dans le shader via une macro d'opération bit à bit (ex: `#define enableMyEffect ((activeEffects & (1u << N)) != 0u)`).
* [LOCAL:ibl] [ANCRÉ] [ARCHI-REVIEW] Transfert de propriété : appel systématique à `ibl_coordinator_get_results` pour extraire les handles et en libérer la responsabilité au niveau du coordinateur.
* [LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] Synchronisation GPU : exécution obligatoire de `glMemoryBarrier` avec le flag de visibilité approprié à la fin de la génération asynchrone des cartes IBL.
* [LOCAL:ibl] [ANCRÉ] [RUNTIME/PROFILER] Rétention d'états : mise en tampon des cartes IBL générées jusqu'à ce que la transition visuelle (Black Screen) soit prête pour le basculement effectif.
* [LOCAL:dependencies] [ANCRÉ] [ARCHI-REVIEW] Trampoline de type : implémentation obligatoire d'une fonction de conversion intermédiaire (`app_render_ui_trampoline(void* user_data)`) pour transtyper les types opaques sans enfreindre la séparation des couches.
* [GLOBAL] [ANCRÉ] [ARCHI-REVIEW] Cycle de vie explicite : chaque structure de données d'un module doit disposer de ses fonctions d'initialisation et de destruction explicites (`init`/`destroy`).
* [GLOBAL] [ANCRÉ] [COMPILATEUR] Portabilité Apple : configuration systématique du flag `GLFW_OPENGL_FORWARD_COMPAT` lors de l'initialisation du contexte GLFW.

## 4. CONVENTIONS DE NOMMAGE, TYPES ET MÉTROLOGIE DU CODE

* [GLOBAL] [ANCRÉ] [GREP] Seuil de complexité : limite stricte de 150 lignes effectives (LOC) par fonction (hors commentaires et macros).
* [GLOBAL] [ANCRÉ] [GREP] Métrologie des fonctions cibles : viser une taille sous la barre des 100 lignes effectives pour les fonctions d'orchestration principales (ex: `scene_render`, `fx_bloom_render`).
* [GLOBAL] [ANCRÉ] [ARCHI-REVIEW] Include fan-out maximum : réduction du fan-out par déportation des inclusions vers les fichiers d'implémentation (`.c`) et usage de déclarations anticipées (forward declarations) dans les en-têtes (`.h`).
* [GLOBAL] [ANCRÉ] [ARCHI-REVIEW] Granularité des modules : découpage obligatoire des modules en unités d'implémentation spécialisées (ex: `postprocess_init.c`, `postprocess_input.c`, `postprocess_apply.c`) pour restreindre la taille physique des fichiers.
* [GLOBAL] [ANCRÉ] [RUNTIME/PROFILER] Tolérance d'analyse statique : conformité stricte aux règles clang-tidy sans aucun avertissement de compilation restant.
* [GLOBAL] [ANCRÉ] [GREP] Espace de nommage symbolique : préfixage obligatoire de l'ensemble des types et fonctions publiques par l'identifiant du module (ex: `app_*`, `postprocess_*`, `pbr_*`, `ibl_coordinator_*`, `billboard_sorter_*`).
* [LOCAL:scene] [ANCRÉ] [COMPILATEUR] Métrologie géométrique : niveau de subdivision de l'icosphere strictement restreint dans l'intervalle d'entiers [0, 6].
* [LOCAL:input] [ANCRÉ] [GREP] Raccourcis de contrôle imposés : C (état caméra), SPACE (position caméra), W (fil de fer), Up/Down (subdivisions), PageUp/PageDown (LOD IBL), F (plein écran), ESC (fermeture).
