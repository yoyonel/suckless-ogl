# Rapport d'Analyse : Gestion de la Mémoire et Shallow Copies

## 1. Contexte de l'Audit

Suite au correctif du commit `9abba91c`, une analyse approfondie de la base de code a été effectuée pour identifier des risques similaires de double libération de mémoire (`double-free`) causés par des copies superficielles (`shallow copies`) de structures gérant des ressources allouées sur le tas (`heap`).

### Le Problème Identifié

Dans `gpu_profiler_ui.c`, la fonction `gpu_profiler_ui_compact_stages` effectuait une affectation par valeur (`*dest = *src`) lors de la compaction des étapes de profilage. La structure `GPUStage` contient deux instances de `AdaptiveSampler`, chacune possédant un pointeur `samples` vers un buffer alloué dynamiquement.

Une copie superficielle de `GPUStage` entraînait le partage de la propriété de ces buffers. Lors du nettoyage final via `gpu_profiler_cleanup`, la fonction tentait de libérer le même pointeur plusieurs fois, provoquant un plantage.

## 2. Analyse du Correctif

Le correctif introduit `gpu_stage_move`, qui :

1. Copie les champs scalaires un par un.
2. **Échange** (`swap`) les pointeurs des buffers des samplers au lieu de les copier.

Cette approche garantit l'unicité du propriétaire pour chaque buffer alloué tout en réutilisant les emplacements mémoire existants dans le tableau `stages`.

## 3. Résultats de l'Audit de la Base de Code

L'audit a porté sur les structures suivantes identifiées comme gérant de la mémoire dynamique :

| Structure | Ressource Critique | État de Risque | Conclusion |
|-----------|-------------------|----------------|------------|
| `GPUStage` | `AdaptiveSampler.samples` | **Corrigé** | Risque maîtrisé par `gpu_stage_move`. |
| `AdaptiveSampler` | `samples` (buffer de données) | Bas | Initialisé par `init`, libéré par `cleanup`. Pas de copie détectée. |
| `AsyncRequest` | `float_data`, `half_data` | Bas | Utilise un pattern de "transfert de propriété destructif" sécurisé dans `async_loader_poll`. |
| `PBRMaterial` | `name` (tableau fixe) | Nul | Structure POD (Plain Old Data). Copie sûre. |
| `MaterialLib` | `materials` (tableau dynamique) | Nul | Géré exclusivement par pointeurs (`MaterialLib*`). Pas de copie par valeur. |
| `SphereInstance` | Aucun (Vecteurs/Matrices) | Nul | Structure POD. Copie sûre (utilisée massivement dans `billboard_sorting.c`). |
| `scene_init_instancing` | instance VBO, billboard VBO, `billboard_instances`, `billboard_sorter` | **Corrigé** | Le chemin de ré-init N-body OFF appelle désormais le cleanup avant la ré-init pour éviter une fuite de ~27 Ko par bascule. |

### Points Particulièrement Examinés

* **`billboard_sorting.c`** : Effectue des copies de `SphereInstance` lors du tri. C'est parfaitement sûr car `SphereInstance` ne contient que des types par valeur (mathématiques cglm).
* **`async_loader.c`** : Le passage de `AsyncRequest` du thread worker au thread principal est sécurisé. Le loader met explicitement à `NULL` ses pointeurs internes après la copie vers le demandeur, évitant ainsi tout conflit de propriété.

## 4. Recommandations et Bonnes Pratiques

1. **Privilégier les Tableaux Fixes pour les Strings** : Comme vu avec `PBRMaterial` et `GPUStage`, l'utilisation de `char name[N]` au lieu de `char* name` élimine les risques liés à la copie de structures.
2. **Move Semantics en C** : Pour les structures complexes (comme `GPUStage`), implémenter systématiquement une fonction de "move" ou de "swap" au lieu d'utiliser l'opérateur `=`.
3. **Documentation de l'Appartenance (`Ownership`)** : Documenter explicitement si une structure "possède" ses pointeurs ou si elle ne fait que les référencer.
4. **Utilisation de l'Analyse Statique** : Continuer à utiliser `just lint-full` (clang-tidy) qui aide à détecter les utilisations après libération (`use-after-free`).

## 5. Conclusion de l'Évaluation

Le risque de double-free lié aux shallow copies de samplers était isolé à la logique de compaction de l'UI du profiler. La cohérence de l'utilisation des pointeurs et la prédominance des structures POD dans le reste du moteur garantissent une bonne stabilité générale de la gestion mémoire.
