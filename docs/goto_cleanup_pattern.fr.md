# Le pattern `goto cleanup` en C

## Le problème : nettoyage en cascade

En C, les fonctions qui acquièrent plusieurs ressources (mémoire, descripteurs de fichiers, objets GL) font face à un dilemme classique : comment tout libérer correctement quand une étape tardive échoue. L'approche naïve — des appels `free()` en cascade à chaque site d'erreur — entraîne un **coût de maintenance en O(n²)** et est une source avérée de fuites mémoire.

```c
/* ❌ Anti-pattern : free en cascade — O(n²) lignes, sujet aux fuites */
int init(App* app)
{
    app->a = calloc(1, sizeof(*app->a));
    if (!app->a) return 0;

    app->b = calloc(1, sizeof(*app->b));
    if (!app->b) {
        free(app->a);           /* 1 free */
        return 0;
    }

    app->c = calloc(1, sizeof(*app->c));
    if (!app->c) {
        free(app->b);           /* 2 frees */
        free(app->a);
        return 0;
    }
    /* Chaque nouvelle allocation ajoute N frees à CHAQUE chemin d'erreur */
    return 1;
}
```

Pire : quand des opérations ultérieures échouent (création du contexte GL, init des sous-systèmes), les développeurs écrivent souvent `return 0` avec **zéro nettoyage**, provoquant silencieusement des fuites de toutes les ressources précédemment allouées.

## La solution : sortie centralisée via `goto`

Un seul bloc de nettoyage à la fin de la fonction, atteint par `goto`, garantit que chaque chemin d'erreur libère les mêmes ressources dans le même ordre :

```c
/* ✅ goto cleanup — O(n) lignes, aucune fuite possible */
int init(App* app)
{
    app->a = calloc(1, sizeof(*app->a));
    if (!app->a) goto cleanup;

    app->b = calloc(1, sizeof(*app->b));
    if (!app->b) goto cleanup;

    app->c = calloc(1, sizeof(*app->c));
    if (!app->c) goto cleanup;

    return 1;

cleanup:
    free(app->c);   /* free(NULL) est sûr (C99 §7.20.3.2) */
    free(app->b);
    free(app->a);
    return 0;
}
```

### Pourquoi `free(NULL)` rend cela sûr

Le standard C garantit que `free(NULL)` est un no-op. Quand `calloc` initialise la struct à zéro, tout pointeur non assigné reste `NULL` et peut être libéré en toute sécurité. Cela élimine le besoin de gardes `if (ptr) free(ptr)`.

## Application : `app_init()` dans suckless-ogl

La fonction `app_init()` utilise une **variante à deux labels** :

| Label | Quand utilisé | Action de nettoyage |
|-------|--------------|-------------------|
| `cleanup_alloc` | Échecs d'allocation avant le contexte GL | `free()` direct des 6 pointeurs calloc'd |
| `cleanup_full` | Échecs après l'init des sous-systèmes (GL actif) | Appelle `app_cleanup()` qui gère le teardown partiel |

**Pourquoi deux labels ?** `app_cleanup()` appelle `app_input_state_cleanup()` et `app_profiling_cleanup()` qui n'ont **pas** de gardes NULL — les appeler sur des pointeurs non initialisés provoquerait un crash. Le label `cleanup_alloc` gère la phase pré-init de manière sûre avec des appels directs `free(NULL)`.

## Comparaison

| Aspect | Free en cascade | goto cleanup |
|--------|----------------|--------------|
| Lignes de nettoyage | O(n²) | O(n) |
| Risque de free oublié | Élevé (chaque chemin d'erreur manuel) | Aucun (chemin unique) |
| Ajout d'une nouvelle allocation | Modifier chaque chemin d'erreur | Ajouter un `free()` au bloc cleanup |
| Lisibilité | Encombré de frees répétitifs | Propre : erreur → goto, cleanup en bas |

## Références

1. **Linux kernel coding style §7** — [kernel.org](https://www.kernel.org/doc/html/latest/process/coding-style.html#centralized-exiting-of-functions) : *« The goto statement comes in handy when a function exits from multiple locations and some common work such as cleanup has to be done. »*
2. **SEI CERT C MEM12-C** — [wiki.sei.cmu.edu](https://wiki.sei.cmu.edu/confluence/display/c/MEM12-C) : *« Consider using a goto chain when leaving a function on error when using and releasing resources. »*
3. **Donald Knuth** — *« Structured Programming with go to Statements »* (1974, Computing Surveys) : analyse formelle montrant que `goto` pour le cleanup est un usage valide et structuré.
4. **Eli Bendersky** — [*« Uses of goto in C »*](https://eli.thegreenplace.net/2009/04/27/using-goto-for-error-handling-in-c) : analyse pratique de `goto` pour le nettoyage de ressources dans de vrais projets C.
5. **Linux Device Drivers, 3e éd.** — O'Reilly : documente `goto` cleanup comme pratique standard dans les fonctions `init`/`exit` des modules kernel.
6. **Stack Overflow** — [*« Valid use of goto for error management in C »*](https://stackoverflow.com/questions/245742) : consensus communautaire avec 500+ votes positifs soutenant le pattern.
