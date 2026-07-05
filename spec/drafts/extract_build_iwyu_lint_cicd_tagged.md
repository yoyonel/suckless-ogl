# Spécifications de Build, Hygiène Statique, CI/CD et Packaging Steam

Ce document rassemble l'ensemble des règles de compilation, de l'hygiène statique, des validations de pipeline CI/CD et des contraintes d'intégration Steam/Proton pour le projet `suckless-ogl`.

Chaque règle est préfixée selon le format : `[SCOPE][MATURITÉ][VÉRIFIABILITÉ]`.

---

## 1. INVARIANTS DE COMPILATION, TOOLCHAIN ET ORCHESTRATION (JUST/CMAKE)

* [LOCAL:build][ANCRÉ][COMPILATEUR] Compiler le projet en C99/C11 sous toolchains GCC ou Clang.
* [LOCAL:build][ANCRÉ][CI-PIPELINE] Exiger au minimum CMake 3.14 pour la configuration et la génération du build.
* [LOCAL:build][ANCRÉ][CI-PIPELINE] Résoudre et lier la dépendance GLFW 3 en tant que bibliothèque système via pkg-config.
* [LOCAL:build][ANCRÉ][COMPILATEUR] Compiler la bibliothèque mathématique cglm statiquement avec l'option `CGLM_STATIC=ON`.
* [LOCAL:build][ANCRÉ][CI-PIPELINE] Générer GLAD dynamiquement durant la phase de configuration CMake pour cibler le profil OpenGL 4.4 Core.
* [LOCAL:build][MVP/TRANSITION][CI-PIPELINE] Exécuter `make deps-setup` ou `just deps-setup` pour cloner et stocker localement les dépendances (`cglm`, `glad`, `stb`, `unity`, `cjson`) dans `deps/` pour le build hors-ligne.
* [LOCAL:build][MVP/TRANSITION][CI-PIPELINE] Router automatiquement les commandes de build et d'analyse statique dans le conteneur Distrobox `clang-dev` si ce dernier est détecté.
* [LOCAL:build][ANCRÉ][CI-PIPELINE] Bloquer l'accès réseau en simulation offline via `make offline-test` en affectant `http_proxy` à une adresse locale invalide.
* [LOCAL:build][ANCRÉ][COMPILATEUR] Injecter obligatoirement le drapeau `-fstack-protector-strong` lors de la compilation pour se prémunir des débordements de pile.
* [LOCAL:build][ANCRÉ][COMPILATEUR] Activer les avertissements de sécurité de format avec `-Wformat -Wformat-security`.
* [LOCAL:build][ANCRÉ][COMPILATEUR] Définir la macro de durcissement `_FORTIFY_SOURCE=2` lors des compilations optimisées.
* [LOCAL:build][ANCRÉ][COMPILATEUR] Durcir l'édition de liens avec les options RELRO, NOW et NoExecStack pour immuniser la table GOT et interdire l'exécution de la pile.
* [LOCAL:build][ANCRÉ][CI-PIPELINE] Accélérer la vitesse de compilation en forçant le parallélisme via `cmake --build build -j$(nproc)`.
* [LOCAL:build][ANCRÉ][GREP] Imposer le format strict d'écriture des logs applicatifs : `YYYY-MM-DD HH:MM:SS,mmm - module - LEVEL - message`.

---

## 2. HYGIÈNE STATIQUE, LINTING ET CONTRÔLE DES INCLUSIONS (IWYU / CLANG-TIDY)

* [LOCAL:lint][ANCRÉ][LINTER] Enforcer l'indentation par caractères Tabulations de largeur équivalente à 8 espaces via la directive `IndentWidth: 8` et `UseTab: ForIndentation`.
* [LOCAL:lint][ANCRÉ][LINTER] Limiter strictement la longueur maximale des lignes de code à 80 colonnes (`ColumnLimit: 80`).
* [LOCAL:lint][ANCRÉ][LINTER] Adopter le style d'accolades Linux/K&R en plaçant l'accolade ouvrante sur la même ligne (`BreakBeforeBraces: Linux`), sauf pour les déclarations de fonctions.
* [LOCAL:lint][ANCRÉ][LINTER] Aligner les déclarations de pointeurs sur le type à gauche via `PointerAlignment: Left`.
* [LOCAL:lint][ANCRÉ][LINTER] Interdire le compactage des fonctions courtes sur une seule ligne via `AllowShortFunctionsOnASingleLine: None`.
* [LOCAL:lint][ANCRÉ][LINTER] Séparer obligatoirement les définitions de fonctions, de structures et d'énumérations par une ligne vide via `SeparateDefinitionBlocks: Always`.
* [LOCAL:lint][ANCRÉ][LINTER] Trier par ordre alphabétique les directives d'inclusion (`SortIncludes: true`) avec priorité pour GLAD.
* [LOCAL:lint][ANCRÉ][LINTER] Utiliser uniquement le style `snake_case` pour les fonctions, variables et membres de structures, et réserver `UPPER_CASE` aux macros.
* [LOCAL:lint][ANCRÉ][LINTER] Bannir les déclarations de `typedef` sur les structures, hormis les pointeurs de fonction et les handles opaques.
* [LOCAL:lint][ANCRÉ][LINTER] Centraliser le traitement des erreurs et la libération des ressources de fonction à l'aide du pattern `goto cleanup`.
* [LOCAL:lint][ANCRÉ][LINTER] Isoler l'implémentation de stb_image dans le fichier dédié `src/stb_image_impl.c` afin de ne pas perturber les règles d'analyse statique globales.
* [LOCAL:lint][ANCRÉ][CI-PIPELINE] Proscrire l'usage des commentaires de suppression de warnings `NOLINT`, `NOLINTNEXTLINE` et `NOLINTBEGIN`/`NOLINTEND` ; chaque signalement doit être résolu à la racine.
* [LOCAL:lint][ANCRÉ][CI-PIPELINE] Intercepter et rejeter tout commit ou PR introduisant des directives `NOLINT` via le script de garde `scripts/check_nolint.sh` ciblant l'index git ou le diff de branche.
* [LOCAL:lint][ANCRÉ][GREP] Justifier obligatoirement toute exception absolue au blocage `NOLINT` par un commentaire, une justification technique, un ticket de tracking associé et un commit forcé avec `--no-verify`.
* [LOCAL:lint][ANCRÉ][CI-PIPELINE] Mettre en cache l'analyse clang-tidy par fichier sentinelle `.linted` dans `.lint_cache/` basé sur le fichier source C, le `.clang-tidy` et `compile_commands.json`.
* [LOCAL:lint][ANCRÉ][LINTER] Désactiver le signalement `MissingIncludes` du check `misc-include-cleaner` dans le fichier `.clang-tidy` pour ignorer les faux positifs transitifs.
* [LOCAL:lint][ANCRÉ][LINTER] Activer le signalement `UnusedIncludes` de `misc-include-cleaner` pour purger les fichiers d'inclusion superflus.
* [LOCAL:lint][ANCRÉ][CI-PIPELINE] Valider la syntaxe et la conformité de tous les shaders `.vert`, `.frag` et `.comp` de `shaders/` au moyen de `glslangValidator` via `scripts/lint_shaders.sh`.
* [LOCAL:lint][ANCRÉ][CI-PIPELINE] Lancer la validation stricte SPIR-V via `just lint-shaders-strict` en exigeant l'écriture de `layout(location=N)` pour les varyings/uniforms et `layout(binding=N)` pour les samplers/images.
* [LOCAL:iwyu][ANCRÉ][LINTER] Lancer IWYU au niveau projet complet via `iwyu_tool` avec les paramètres d'analyse C `-Xiwyu --no_fwd_decls`.
* [LOCAL:iwyu][ANCRÉ][GREP] Exclure de l'analyse automatique les faux positifs d'inclusions documentés (`gl_common.h`, `cglm/cglm.h`, `sched.h`, etc.) via l'allowlist interne du script `scripts/iwyu_check.sh`.
* [LOCAL:iwyu][ANCRÉ][LINTER] Déclarer les équivalences d'inclusion (ex: mapper `glad/glad.h` vers son enveloppe `gl_common.h`) dans la configuration `.iwyu.imp`.
* [LOCAL:iwyu][ANCRÉ][CI-PIPELINE] Valider l'absence d'inclusions superflues sur les fichiers modifiés en pre-push avec `just iwyu-check`.
* [GLOBAL][ANTI-PATTERN][LINTER] Proscrire le recours implicite aux inclusions transitives ; tout fichier consommateur doit directement importer les en-têtes définissant les symboles exploités.

---

## 3. RÈGLES DU PIPELINE CI/CD ET VALIDATION AUTOMATISÉE

* [LOCAL:cicd][ANCRÉ][CI-PIPELINE] Déclencher automatiquement le workflow GitHub Actions sur push vers `master`/`main`, création de pull request, tag de version `v*` ou via le déclencheur cron planifié chaque jour à 01:00 UTC.
* [LOCAL:cicd][ANCRÉ][CI-PIPELINE] Mettre en échec le job de vérification de qualité CI si l'application de `make format` entraîne une modification de fichier visible via `git diff`.
* [LOCAL:cicd][ANCRÉ][CI-PIPELINE] Exécuter les tests unitaires et d'intégration OpenGL dans un environnement d'affichage virtuel headless via `xvfb-run` / `run_test_with_xvfb.sh`.
* [LOCAL:cicd][ANCRÉ][CI-PIPELINE] Valider l'intégrité visuelle du moteur en capturant 6 vues de rendu et en les confrontant par régression à des images PNG de référence.
* [LOCAL:cicd][ANCRÉ][CI-PIPELINE] Produire automatiquement le rapport de comparaison visuel interactif au survol et le poster en commentaire de pull request via `generate_visual_report.py`.
* [LOCAL:cicd][ANCRÉ][CI-PIPELINE] Publier un aperçu de la documentation technique sous `pr-preview/pr-<N>/` sur GitHub Pages pour chaque PR active.
* [LOCAL:cicd][ANCRÉ][CI-PIPELINE] Générer lors des builds de production les trois exécutables distincts : `app-Release` (`-O2`/`-O3`), `app-Profiling` (avec symboles de profilage) et `app-UltraRelease` (`-DENABLE_UNITY_BUILD`, `-march=native`, `-ffast-math`).
* [LOCAL:cicd][ANCRÉ][CI-PIPELINE] Publier et écraser automatiquement les livrables de nuit sur le tag `nightly` sans passer par un état draft/brouillon.

---

## 4. CONTRAINTES DE PACKAGING ET PORTABILITÉ (WINDOWS / STEAM / PROTON)

* [LOCAL:packaging][ANCRÉ][CI-PIPELINE] Compiler le livrable exécutable Windows (.exe) à destination exclusive de l'architecture Win64 via MinGW-w64 (`just build-win` / `just configure-win`).
* [LOCAL:packaging][ANCRÉ][COMPILATEUR] Détecter et localiser dynamiquement le dossier racine des ressources au démarrage du programme via `platform_setup_working_dir` en changeant le CWD pour cibler le dossier de l'exécutable ou remonter jusqu'à 4 répertoires parents.
* [LOCAL:packaging][ANCRÉ][COMPILATEUR] Intercepter impérativement les flux `stdout` et `stderr` au démarrage via `freopen` vers `suckless_output.log` et `suckless_crash.log` pour parer à la disparition de la console induite par le flag `-mwindows`.
* [LOCAL:packaging][ANCRÉ][CI-PIPELINE] Archiver la distribution de build Windows dans un conteneur compressé Zstandard (`.tar.zst`) via `just package-win`.
* [LOCAL:packaging][ANCRÉ][CI-PIPELINE] Tester localement le package Windows extrait via `just run-package-win` pour assurer le bon fonctionnement de `scripts/run_proton.sh` sous Proton Flatpak ou Natif.
* [LOCAL:packaging][ANCRÉ][CI-PIPELINE] Isoler le préfixe de test local Proton dans le dossier `test-dist/proton_pfx` hors de l'arborescence Steam hôte afin d'éviter les pollutions d'environnement.
* [LOCAL:steam][ANCRÉ][CI-PIPELINE] Exposer l'arborescence du projet hôte au conteneur Steam Flatpak au moyen de la directive `--filesystem` et renseigner explicitement les variables d'environnement `STEAM_COMPAT_CLIENT_INSTALL_PATH` et `STEAM_COMPAT_DATA_PATH`.
* [LOCAL:steam][ANTI-PATTERN][CI-PIPELINE] Proscrire l'usage des dossiers virtuels du portail XDG Desktop lors de l'ajout du raccourci dans l'interface Steam (le montage en lecture seule FUSE à l'adresse `/run/user/1000/doc/...` bloque la résolution de dossier et corrompt la lecture en retournant l'erreur Win32 193).
* [LOCAL:steam][ANCRÉ][CI-PIPELINE] Renseigner physiquement le chemin de l'exécutable de jeu depuis la racine `/home` lors de la navigation dans l'explorateur Steam client.
* [LOCAL:steam][ANCRÉ][CI-PIPELINE] Entourer systématiquement de guillemets doubles `" "` les chemins d'accès renseignés dans les champs "Cible" et "Démarrer dans" des propriétés du raccourci Steam.
* [LOCAL:steam][ANCRÉ][CI-PIPELINE] Autoriser l'affichage de MangoHud sur les builds OpenGL Windows sous Proton en forçant le préchargement de librairies via l'option de lancement : `STEAM_COMPAT_MOUNTS="/absolute/path/to/test-dist/" WINEPREFIX="%compat%" mangohud %command%`.
* [LOCAL:steam][ANCRÉ][CI-PIPELINE] Identifier et lier les visuels personnalisés au raccourci de jeu Non-Steam en calculant le hash CRC32 de son chemin binaire brut dans `shortcuts.vdf`.
* [LOCAL:steam][ANCRÉ][CI-PIPELINE] Supprimer le dossier cache agressif `appcache/librarycache/*` après fermeture de Steam afin de forcer le rechargement des visuels modifiés.
* [LOCAL:steam][ANCRÉ][CI-PIPELINE] Enforcer le format d'image `.ico` pour l'icône de raccourci associée au binaire Windows, le format PNG risquant d'échouer selon les Proton et conteneurs Steam.
* [LOCAL:steam][ANCRÉ][CI-PIPELINE] Outrepasser les restrictions d'accès du bac à sable Flatpak Steam en exécutant l'instruction de permission : `flatpak override --user --filesystem=/votre/chemin/de/projet com.valvesoftware.Steam`.
