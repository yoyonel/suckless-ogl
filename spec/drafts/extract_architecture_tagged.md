# Règles d'Architecture Extrayant `docs/architecture.md`

## 1. INVARIANTS
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] L'initialisation et la libération de l'état de l'orchestrateur (`App`) doivent s'exécuter via une table de descripteurs de sous-systèmes (`APP_SUBSYSTEM_TABLE`) terminée par la sentinelle `{0}`.
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] Le sous-système de gestion de la fenêtre graphique (`APP_WINDOW_DESCRIPTOR`) doit obligatoirement figurer en premier dans la table d'initialisation pour instancier le contexte OpenGL requis par les autres modules.
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] La libération des sous-systèmes doit s'effectuer dans l'ordre strictement inverse de leur initialisation.
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] En cas d'échec d'initialisation du sous-système d'index $N$, le mécanisme de rollback automatique doit libérer les sous-systèmes déjà initialisés de l'index $N-1$ à $0$ dans l'ordre inverse.
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] Le contexte OpenGL doit rester valide tout au long du nettoyage des ressources GPU, impliquant la destruction de la fenêtre en dernier lieu.
* [LOCAL:app][ANCRÉ][COMPILATEUR] Les en-têtes des modules fonctionnels doivent être inclus à la fin de `app.h` pour garantir la visibilité de la définition complète de la structure `App`.
* [LOCAL:app_ui][ANCRÉ][GREP] Les données de layout statiques de l'interface utilisateur doivent demeurer privées à l'unité de compilation `app_ui.c`.

## 2. INTERDICTIONS FORMELLES
* [GLOBAL][ANTI-PATTERN][COMPILATEUR] Interdiction de définir les structures de données clés (`App`, `Camera`, `AsyncRequest`, `Shader`, `NBodySim`, `SphereInstance`) sous forme de structures anonymes afin de ne pas bloquer les déclarations anticipées.
* [GLOBAL][ANTI-PATTERN][ARCHI-REVIEW] Interdiction d'inclure un fichier d'en-tête complet dans un fichier `.h` si le type défini n'est manipulé que par pointeur ou par référence.
* [LOCAL:postprocess][ANTI-PATTERN][GREP] Interdiction d'inclure `postprocess.h` dans les fichiers sources d'implémentation des effets de post-traitement (`fx_*.c`).
* [GLOBAL][ANTI-PATTERN][GREP] Interdiction de déclarer des tableaux `static const` partagés directement dans les en-têtes (notamment les constantes de layout de l'interface utilisateur).
* [GLOBAL][ANTI-PATTERN][GREP] Interdiction de concevoir des en-têtes de type "feuille" avec plus de 5 inclusions d'en-têtes de projet.
* [LOCAL:app][ANTI-PATTERN][GREP] Interdiction de définir un paramètre de taille ou de décompte explicite pour la table des sous-systèmes.

## 3. PATTERNS OBLIGATOIRES
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] Décomposer les structures volumineuses comme `Scene` et `PostProcess` en sous-structures typées et sous-en-têtes dédiés alignés par domaine fonctionnel.
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] Gérer le cycle de vie de l'orchestrateur via le patron de descripteurs de sous-systèmes (`SubsystemDescriptor` avec pointeurs `init` et `cleanup`).
* [GLOBAL][ANCRÉ][GREP] Annoter systématiquement chaque fonction d'initialisation et de libération de sous-système avec le commentaire de traçabilité statique : `/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */`.
* [LOCAL:app_input][ANCRÉ][ARCHI-REVIEW] Découpler l'acquisition d'entrées physiques de la simulation de la caméra via le patron Bridge en utilisant `GamepadContext`.
* [LOCAL:postprocess][ANCRÉ][COMPILATEUR] Injecter le contexte d'effet en lecture seule `EffectContext` sous la forme `(FX*, Params*, const EffectContext*)` pour découpler les effets individuels de l'objet global `PostProcess`.
* [GLOBAL][ANCRÉ][ARCHI-REVIEW] Scinder les modules de calcul ou de rendu complexes (ex: `postprocess.c`) en unités de compilation distinctes, orchestrées par un en-tête d'interface interne (ex: `postprocess_internal.h`).
* [LOCAL:app][ANCRÉ][ARCHI-REVIEW] Appliquer la procédure d'ajout de sous-système en quatre étapes : implémenter le couple init/cleanup dans le fichier `.c`, déclarer la macro du descripteur dans le fichier `.h`, référencer la macro dans `APP_SUBSYSTEM_TABLE` en respectant l'ordre de dépendance, et implémenter le test unitaire de cycle de vie dans `tests/test_app_subsystem.c`.

## 4. CONVENTIONS DE TYPES ET NOMMAGE
* [GLOBAL][ANCRÉ][GREP] Nommer toutes les fonctions de cycle de vie des sous-systèmes avec le suffixe unique `_subsys_init` et `_subsys_cleanup`.
* [GLOBAL][ANCRÉ][GREP] Préfixer toutes les fonctions internes et partagées d'un module par le trigramme associé (ex: `pp_` pour le post-traitement).
* [LOCAL:postprocess][ANCRÉ][GREP] Préfixer les identifiants d'énumérations d'unités de texture du post-traitement par `POSTPROCESS_TEX_UNIT_`.
* [LOCAL:app][ANCRÉ][GREP] Nommer chaque macro de description de sous-système sous la forme `APP_[NOM_MODULE]_DESCRIPTOR`.
* [GLOBAL][ANCRÉ][GREP] Déclarer les types de base sous forme de structures nommées (`typedef struct X X;`).
* [LOCAL:app_input][ANCRÉ][GREP] Nommer l'en-tête de définition de la structure d'un sous-système d'entrée avec le suffixe `_state` (ex: `app_input_state.h`) pour le distinguer de l'en-tête de contexte d'entrée d'interface (ex: `app_input.h`).
* [GLOBAL][MVP/TRANSITION][GREP] Limite de 500 lignes de code (LOC) par module : un module dépassant 500 LOC est un candidat prioritaire à la décomposition.
