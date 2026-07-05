# Constitution R&D : Règles de Profilage, Benchmarking et Audit GDB

## 1. INVARIANTS DU PROFILAGE TEMPS RÉEL (TRACY CPU/GPU & MACROS)
- [LOCAL:tracy] [ANCRÉ] [COMPILATEUR] Activer le profiler dans le build via l'option CMake `-DENABLE_TRACY=ON`.
- [LOCAL:tracy] [ANCRÉ] [COMPILATEUR] Compiler le profiler Tracy Linux avec `-DLEGACY=ON` pour utiliser le backend X11/GLFW.
- [LOCAL:tracy] [ANCRÉ] [GREP] Nommer explicitement le thread principal et les threads de travail (ex: `"HDR I/O Thread"`) et émettre `FrameMark` à chaque frame.
- [LOCAL:gpu_profiler] [ANCRÉ] [PROFILER] Utiliser des requêtes de timing OpenGL double-bufférisées (lecture frame N-1 à la frame N) couplées à une attente bloquante `glGetQueryObjectui64v` pour garantir la continuité des mesures.
- [LOCAL:gpu_profiler] [ANCRÉ] [COMPILATEUR] Rendre le profilage GPU conditionnel à la visibilité de l'UI (`F3`), au logging (`F4`) ou à un benchmark actif pour éliminer l'overhead hors-profilage.
- [LOCAL:gpu_profiler] [ANCRÉ] [GREP] Déclarer et fermer les zones de profilage GPU via la macro RAII `GPU_STAGE_PROFILER`.
- [LOCAL:tracy] [ANCRÉ] [PROFILER] Analyser les zones hybrides en distinguant le temps d'enregistrement des commandes (Host, rouge) et le temps d'attente de la frame précédente (Sync, vert).
- [LOCAL:gpu_usage] [ANCRÉ] [GREP] Calculer le taux d'utilisation GPU via la lecture de `/proc/self/fdinfo/` sur Linux à un intervalle strict de 500 ms (`METRICS_UPDATE_PERIOD_MS`).
- [LOCAL:gpu_usage] [ANCRÉ] [GREP] Dédoublonner les descripteurs de fichier DRM via `drm-client-id` pour éviter les surévaluations de charge GPU.
- [LOCAL:gpu_usage] [MVP/TRANSITION] [COMPILATEUR] Désactiver le moniteur fdinfo (`available = false`) sur Windows et macOS via des stubs inactifs.

## 2. PROTOCOLES STRICTS DE BENCHMARKING ET MÉTROLOGIE RENDERDOC
- [LOCAL:benchmark] [ANCRÉ] [BENCHMARK-PROTOCOL] Exécuter 10 itérations de l'application après un préchauffage strict de 128 frames (`warm-up`) avant toute mesure de performance statistique.
- [LOCAL:benchmark] [ANCRÉ] [BENCHMARK-PROTOCOL] Identifier les gains de performance selon trois catégories exclusives : Direct Shader Gain, Sync Tax Reduction, et Ghost Gains.
- [LOCAL:benchmark] [ANCRÉ] [BENCHMARK-PROTOCOL] Mesurer le coût individuel d'un effet post-processus par la méthode A/B (`Cost = T(all ON) - T(effect OFF)`) sur 120 frames précédées de 30 frames de warm-up.
- [LOCAL:benchmark] [ANCRÉ] [BENCHMARK-PROTOCOL] Rejeter comme statistiquement non significatif tout gain mesuré inférieur à la déviation standard (`|cost| < stddev`).
- [LOCAL:renderdoc] [ANCRÉ] [PROFILER] Configurer RenderDoc avec `Capture Child Processes` et `Ref All Resources` actifs.
- [LOCAL:renderdoc] [ANCRÉ] [COMPILATEUR] Instrumenter le code avec `GL_KHR_debug` : labelliser chaque ressource via `glObjectLabel` et découper le frame-graph via `glPushDebugGroup` / `glPopDebugGroup`.
- [LOCAL:renderdoc] [ANCRÉ] [COMPILATEUR] Assurer la compatibilité SPIR-V pour RenderDoc en décorant tous les inter-stages et uniforms non-opaques avec `layout(location = N)` et les opaques avec `layout(binding = N)`.
- [LOCAL:renderdoc] [ANCRÉ] [COMPILATEUR] Réserver 4 locations consécutives pour chaque variable de type `mat4` dans les déclarations d'uniforms.
- [LOCAL:renderdoc] [ANCRÉ] [COMPILATEUR] Proscrire l'usage de `gl_DepthRange` dans les shaders, le remplacer par sa constante statique pour le range [0, 1] : `(ndcDepth + 1.0) * 0.5`.
- [LOCAL:renderdoc] [ANCRÉ] [PROFILER] Lancer le débogage de fragment shader depuis le panneau Pixel History sur une ligne de fragment valide et validée pour éviter l'échec d'évaluation ("Pixel shader debug failed").

## 3. RÈGLES D'AUDIT DE PERFORMANCE, FLAMEGRAPHS ET DIAGNOSTIC GDB
- [LOCAL:profiling] [ANCRÉ] [COMPILATEUR] Compiler en mode `RelWithDebInfo` ou `Profiling` avec `-fno-omit-frame-pointer` et `-g` pour préserver le call stack lors de l'échantillonnage perf.
- [GLOBAL] [ANCRÉ] [BENCHMARK-PROTOCOL] Configurer `kernel.perf_event_paranoid=1` et `kernel.kptr_restrict=0` sur le système hôte pour autoriser perf à échantillonner les appels système et pilotes graphiques.
- [LOCAL:profiling] [ANCRÉ] [BENCHMARK-PROTOCOL] Échantillonner la performance CPU avec `perf record -g --call-graph dwarf` pour garantir une résolution DWARF robuste.
- [LOCAL:profiling] [ANCRÉ] [PROFILER] Générer le FlameGraph en combinant `perf script`, `stackcollapse-perf.pl` et `flamegraph.pl` pour identifier visuellement les plateaux CPU.
- [LOCAL:profiling] [ANCRÉ] [BENCHMARK-PROTOCOL] Échantillonner les tests headless sous xvfb en utilisant la commande de capture `perf record -g xvfb-run -a -s "-screen 0 1024x768x24"`.
- [LOCAL:gdb] [ANCRÉ] [BENCHMARK-PROTOCOL] Diagnostiquer les plantages de manière automatisée en batch-mode via `gdb -batch -ex run -ex 'bt full'`.
- [LOCAL:gdb] [ANCRÉ] [PROFILER] Analyser les plantages d'optimisation en mode `RelWithDebInfo` car les optimisations agressives (`-O3`) d'auto-vectorisation SIMD n'apparaissent pas à bas niveau d'optimisation (`-O0`).
- [LOCAL:gdb] [ANCRÉ] [PROFILER] Inspecter l'adresse de crash en assembleur avec `x/20i $pc-40` et vérifier l'alignement des registres de destination avec `p/x $register % 32` (ou 64).
- [LOCAL:gdb] [ANCRÉ] [COMPILATEUR] Bannir `calloc`/`malloc` au profit de `posix_memalign` (alignement de 64 octets, `SIMD_ALIGNMENT`) pour toutes les structures contenant des données traitées par instructions vectorisées AVX/AVX-512.
- [GLOBAL] [ANCRÉ] [BENCHMARK-PROTOCOL] Exécuter les binaires de test de performance avec la commande `timeout 5` pour éviter les blocages de processus infinis en environnement CI/CD.
- [LOCAL:gdb] [ANCRÉ] [BENCHMARK-PROTOCOL] Activer la génération de core dumps via `ulimit -c unlimited` pour l'analyse post-mortem différée avec `gdb ./build/app core`.

## 4. INTERDICTIONS FORMELLES EN MESURE ET OPTIMISATION (ANTI-PATTERNS)
- [GLOBAL] [ANTI-PATTERN] [BENCHMARK-PROTOCOL] Mesurer des performances ou exécuter des protocoles de benchmarking sur un build configuré en mode Debug.
- [GLOBAL] [ANTI-PATTERN] [BENCHMARK-PROTOCOL] Ignorer la phase de préchauffage (warm-up) de 128 frames, ce qui expose les mesures aux instabilités DVFS ou JIT du driver.
- [GLOBAL] [ANTI-PATTERN] [BENCHMARK-PROTOCOL] Utiliser des horloges CPU (comme `glfwGetTime`) pour évaluer le temps d'exécution de passes GPU au lieu de requêtes matérielles `glQueryCounter`.
- [GLOBAL] [ANTI-PATTERN] [PROFILER] Effectuer des requêtes synchrones immédiates `glGetQueryObjectui64v` sur le thread de rendu principal au cours du frame courant, provoquant un blocage de pipeline.
- [LOCAL:profiling] [ANTI-PATTERN] [BENCHMARK-PROTOCOL] Interagir avec la caméra ou la scène (overdraw, occlusion) pendant le déroulement d'une série statistique de benchmarking.
- [LOCAL:renderdoc] [ANTI-PATTERN] [COMPILATEUR] Soumettre des groupes de débogage `GL_KHR_debug` ou demander un contexte `GLFW_OPENGL_DEBUG_CONTEXT` sur un build configuré pour la Release.
- [LOCAL:gpu_usage] [ANTI-PATTERN] [GREP] Lire les données de `/proc/self/fdinfo/` sans filtrer par `drm-client-id`, entraînant une double comptabilisation du temps d'utilisation GPU.
- [LOCAL:benchmark] [ANTI-PATTERN] [BENCHMARK-PROTOCOL] Additionner linéairement le coût de chaque effet post-processus issu d'un uber-shader composite pour en déduire le coût global.
- [LOCAL:renderdoc] [ANTI-PATTERN] [PROFILER] Déboguer un pixel à partir de l'Event Browser sans valider son passage aux tests de profondeur, de stencil et de discard via l'historique de pixel (Pixel History).
- [LOCAL:renderdoc] [ANTI-PATTERN] [COMPILATEUR] Écrire des expressions de profondeur basées sur `gl_DepthRange` dans les shaders, provoquant un échec de compilation SPIR-V.
- [GLOBAL] [ANTI-PATTERN] [PROFILER] Modifier arbitrairement la logique d'ordonnancement de la boucle d'application sans preuve empirique de pipeline bubble fournie par le profiler.
- [LOCAL:gpu_profiler] [ANTI-PATTERN] [GREP] Laisser les Timer Queries OpenGL s'exécuter lorsque l'interface du profileur ou le benchmark d'effets sont inactifs.
