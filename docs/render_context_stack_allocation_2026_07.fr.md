# Analyse de l'Allocation sur la Pile du RenderContext (Juillet 2026)

**Date** : 3 juillet 2026
**Statut** : Patron de Conception Documenté

## Contexte

Au cœur de la boucle principale du moteur (`app_run()` dans `src/app.c`), une structure locale `RenderContext` est instanciée et initialisée à chaque frame avant d'appeler `renderer_draw_frame()` :

```c
{
    PROFILE_ZONE(render_ctx, "App Render");
    RenderContext rctx = {
        .scene = app->scene,
        .postprocess = app->postprocess,
        .camera = &app->input->camera,
        .profiler = &app->profiling->gpu_profiler,
        .profiler_ui = &app->profiling->timeline_ui,
        .env_mgr = app->env_mgr,
        .notifier = &app->notifier,
        .effect_bench = &app->effect_bench,
        .width = app->width,
        .height = app->height,
        .delta_time = app->delta_time,
        .frame_count = app->frame_count,
        .log_gpu_metrics = app->profiling->log_gpu_metrics,
        .render_ui = app_render_ui_trampoline,
        .render_ui_data = app,
    };
    renderer_draw_frame(&rctx);
    PROFILE_ZONE_END(render_ctx);
}
```

Cette note technique documente les motivations architecturales et vérifie les implications de performance (CPU, cache et mémoire) de ce patron au sein de la boucle rapide (hot loop) de l'application.

---

## 1. Motivations Architecturales

La raison principale d'utiliser une structure de contexte temporaire réside dans la **Séparation des Préoccupations (SoC)** et le **Découplage** :

1. **Découplage vis-à-vis de l'orchestrateur `App`** :
   * Le sous-système de rendu bas niveau (`renderer.c` / `renderer.h`) ne doit pas dépendre de la structure globale `App`. Si `renderer_draw_frame` acceptait directement un `App*`, `renderer.h` devrait inclure `app.h`, ce qui introduirait des dépendances circulaires et polluerait l'architecture.
   * `RenderContext` représente un objet paramètre (Parameter Object) propre ne contenant que les éléments requis par le renderer.
2. **Gestion unifiée des paramètres statiques et dynamiques** :
   * Bien que certains pointeurs soient constants (ex. `app->scene`), plusieurs champs changent **dynamiquement à chaque frame** :
     * `.delta_time` et `.frame_count`
     * `.width` et `.height` (redimensionnement dynamique de la fenêtre)
     * `.log_gpu_metrics` (paramètres dynamiques de profilage utilisateur)
3. **Durée de vie des pointeurs et sécurité** :
   * Construire le contexte à la demande garantit la fraîcheur des pointeurs (Source Unique de Vérité), éliminant tout risque de pointeurs obsolètes (stale pointers) si des sous-systèmes venaient à être réalloués ou rechargés dynamiquement en cours de route.

---

## 2. Implications en matière de Performances (Audit de la Boucle Rapide)

Reconstruire cette structure sur la pile (stack) à chaque frame est **virtuellement gratuit** pour le CPU et le cache :

### A. Coût de l'allocation sur la pile
Les variables locales en C résident sur la pile du programme. L'allocation ne fait pas intervenir le noyau du système d'exploitation ni de gestionnaire de tas (heap).
* **Coût d'instruction** : Le compilateur réserve de l'espace sur la pile en ajustant le pointeur de pile (ex. l'instruction assembleur `sub rsp, size`). Cet ajustement a lieu une seule fois lors de l'entrée dans la fonction ou le bloc et s'exécute en un seul cycle CPU.

### B. Taille et empreinte de cache
La structure `RenderContext` contient 10 pointeurs (80 octets sur les systèmes 64 bits) et 5 champs scalaires/callbacks.
* **Taille totale** : Environ **112 octets** (avec l'alignement de la structure).
* **Utilisation des lignes de cache** : Cette taille tient dans moins de **deux lignes de cache CPU L1** (qui font 64 octets chacune).
* **Efficacité du cache L1** : L'orchestrateur `App` et le cadre de pile (stack frame) étant extrêmement actifs, les champs sources et les adresses cibles sont déjà présents dans le cache de données L1, ce qui réduit les opérations de copie à quelques cycles processeur.

### C. Optimisations du compilateur (O2 / O3)
Les compilateurs modernes optimisent agressivement cette configuration :
1. **Passage par référence** : La structure est passée à `renderer_draw_frame(&rctx)` par pointeur. Dans l'ABI System V AMD64, ce pointeur est transmis via un seul registre CPU (`rdi`), évitant ainsi de copier les valeurs de la structure lors de l'appel.
2. **Allocation dans des registres & Inlining** : Si le compilateur inline `renderer_draw_frame()`, la structure intermédiaire `rctx` est entièrement éliminée par optimisation. Le compilateur mappera directement les champs vers les registres ou les emplacements de pile des appels cibles, réalisant ainsi zéro copie.

---

## 3. Alternatives de Conception Envisagées

* **Alternative A : Mettre en cache `RenderContext` dans `App`**
  * *Inconvénient* : Cela éviterait de copier les pointeurs statiques, mais nécessiterait toujours de mettre à jour les variables dynamiques (`.width`, `.height`, `.delta_time`, `.frame_count`) à chaque frame. Cela ajouterait une surcharge de synchronisation d'état et exposerait à des risques d'obsolescence des pointeurs si des sous-systèmes étaient réalloués.
* **Alternative B : Passer les paramètres sous forme d'arguments individuels**
  * *Inconvénient* : `renderer_draw_frame(app->scene, app->postprocess, ...)` nécessiterait plus de 15 arguments. Cela rendrait le code difficile à maintenir et violerait les conventions ABI (qui limitent à 6 les arguments transmis par registres ; les autres étant empilés sur la pile, générant ainsi plus de surcharge qu'un simple pointeur de structure propre).
