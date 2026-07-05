# Règles d'Architecture Extrayant `docs/architecture.md`

## 1. INVARIANTS
* L'initialisation et la libération de l'état de l'orchestrateur (`App`) doivent s'exécuter via une table de descripteurs de sous-systèmes (`APP_SUBSYSTEM_TABLE`) terminée par la sentinelle `{0}`.
* Le sous-système de gestion de la fenêtre graphique (`APP_WINDOW_DESCRIPTOR`) doit obligatoirement figurer en premier dans la table d'initialisation pour instancier le contexte OpenGL requis par les autres modules.
* La libération des sous-systèmes doit s'effectuer dans l'ordre strictement inverse de leur initialisation.
* En cas d'échec d'initialisation du sous-système d'index $N$, le mécanisme de rollback automatique doit libérer les sous-systèmes déjà initialisés de l'index $N-1$ à $0$ dans l'ordre inverse.
* Le contexte OpenGL doit rester valide tout au long du nettoyage des ressources GPU, impliquant la destruction de la fenêtre en dernier lieu.
* Les en-têtes des modules fonctionnels doivent être inclus à la fin de `app.h` pour garantir la visibilité de la définition complète de la structure `App`.
* Les données de layout statiques de l'interface utilisateur doivent demeurer privées à l'unité de compilation `app_ui.c`.

## 2. INTERDICTIONS FORMELLES
* Interdiction de définir les structures de données clés (`App`, `Camera`, `AsyncRequest`, `Shader`, `NBodySim`, `SphereInstance`) sous forme de structures anonymes afin de ne pas bloquer les déclarations anticipées.
* Interdiction d'inclure un fichier d'en-tête complet dans un fichier `.h` si le type défini n'est manipulé que par pointeur ou par référence.
* Interdiction d'inclure `postprocess.h` dans les fichiers sources d'implémentation des effets de post-traitement (`fx_*.c`).
* Interdiction de déclarer des tableaux `static const` partagés directement dans les en-têtes (notamment les constantes de layout de l'interface utilisateur).
* Interdiction de concevoir des en-têtes de type "feuille" avec plus de 5 inclusions d'en-têtes de projet.
* Interdiction de définir un paramètre de taille ou de décompte explicite pour la table des sous-systèmes.

## 3. PATTERNS OBLIGATOIRES
* Décomposer les structures volumineuses comme `Scene` et `PostProcess` en sous-structures typées et sous-en-têtes dédiés alignés par domaine fonctionnel.
* Gérer le cycle de vie de l'orchestrateur via le patron de descripteurs de sous-systèmes (`SubsystemDescriptor` avec pointeurs `init` et `cleanup`).
* Annoter systématiquement chaque fonction d'initialisation et de libération de sous-système avec le commentaire de traçabilité statique : `/* Called via APP_SUBSYSTEM_TABLE in app.c (subsystem descriptor pattern) */`.
* Découpler l'acquisition d'entrées physiques de la simulation de la caméra via le patron Bridge en utilisant `GamepadContext`.
* Injecter le contexte d'effet en lecture seule `EffectContext` sous la forme `(FX*, Params*, const EffectContext*)` pour découpler les effets individuels de l'objet global `PostProcess`.
* Scinder les modules de calcul ou de rendu complexes (ex: `postprocess.c`) en unités de compilation distinctes, orchestrées par un en-tête d'interface interne (ex: `postprocess_internal.h`).
* Appliquer la procédure d'ajout de sous-système en quatre étapes : implémenter le couple init/cleanup dans le fichier `.c`, déclarer la macro du descripteur dans le fichier `.h`, référencer la macro dans `APP_SUBSYSTEM_TABLE` en respectant l'ordre de dépendance, et implémenter le test unitaire de cycle de vie dans `tests/test_app_subsystem.c`.

## 4. CONVENTIONS DE TYPES ET NOMMAGE
* Nommer toutes les fonctions de cycle de vie des sous-systèmes avec le suffixe unique `_subsys_init` et `_subsys_cleanup`.
* Préfixer toutes les fonctions internes et partagées d'un module par le trigramme associé (ex: `pp_` pour le post-traitement).
* Préfixer les identifiants d'énumérations d'unités de texture du post-traitement par `POSTPROCESS_TEX_UNIT_`.
* Nommer chaque macro de description de sous-système sous la forme `APP_[NOM_MODULE]_DESCRIPTOR`.
* Déclarer les types de base sous forme de structures nommées (`typedef struct X X;`).
* Nommer l'en-tête de définition de la structure d'un sous-système d'entrée avec le suffixe `_state` (ex: `app_input_state.h`) pour le distinguer de l'en-tête de contexte d'entrée d'interface (ex: `app_input.h`).
