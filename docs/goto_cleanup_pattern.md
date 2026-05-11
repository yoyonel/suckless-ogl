# The `goto cleanup` Pattern in C

## The Problem: Cascading Cleanup

In C, functions that acquire multiple resources (memory, file handles, GL objects) face a classic dilemma: how to release everything correctly when a late step fails. The naive approach — cascading `free()` calls at each error site — leads to **O(n²) maintenance cost** and is a proven source of memory leaks.

```c
/* ❌ Anti-pattern: cascading free — O(n²) lines, leak-prone */
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
    /* Each new allocation adds N frees to EVERY error path above */
    return 1;
}
```

Worse: when later operations fail (GL context creation, subsystem init), developers often write `return 0` with **zero cleanup**, silently leaking all previously allocated resources.

## The Solution: Centralized Exit via `goto`

A single cleanup block at the end of the function, reached by `goto`, guarantees that every error path releases the same resources in the same order:

```c
/* ✅ goto cleanup — O(n) lines, no leaks possible */
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
    free(app->c);   /* free(NULL) is safe (C99 §7.20.3.2) */
    free(app->b);
    free(app->a);
    return 0;
}
```

### Why `free(NULL)` Makes This Safe

The C standard guarantees that `free(NULL)` is a no-op. When `calloc` zero-initializes the struct, any pointer that was never assigned remains `NULL` and can be safely freed. This eliminates the need for `if (ptr) free(ptr)` guards.

## Applied: `app_init()` in suckless-ogl

The `app_init()` function uses a **single-label** `goto cleanup_full` pattern:

| Label | When Used | Cleanup Action |
|-------|-----------|---------------|
| `cleanup_full` | Failures after subsystem descriptor init (GL active) | Calls `app_cleanup()` which handles partial teardown |

The previous `cleanup_alloc` label (for pre-GL allocation failures) was removed when subsystem descriptors took over. The descriptor table's `app_subsystems_init()` now handles rollback automatically: on the first failure at index *N*, it calls `cleanup()` in reverse for entries *[N-1 .. 0]*, freeing any `calloc`'d sub-structs. Only post-descriptor Phase 3 failures (scene, async loader, post-processing) still use `goto cleanup_full`.

## Comparison

| Aspect | Cascading Free | goto cleanup |
|--------|---------------|--------------|
| Lines of cleanup code | O(n²) | O(n) |
| Risk of missed free | High (each error path manual) | None (single path) |
| Adding a new allocation | Touch every error path above | Add one `free()` to cleanup block |
| Readability | Cluttered with repetitive frees | Clean: error → goto, cleanup at bottom |

## References

1. **Linux kernel coding style §7** — [kernel.org](https://www.kernel.org/doc/html/latest/process/coding-style.html#centralized-exiting-of-functions): *"The goto statement comes in handy when a function exits from multiple locations and some common work such as cleanup has to be done."*
2. **SEI CERT C MEM12-C** — [wiki.sei.cmu.edu](https://wiki.sei.cmu.edu/confluence/display/c/MEM12-C): *"Consider using a goto chain when leaving a function on error when using and releasing resources."*
3. **Donald Knuth** — *"Structured Programming with go to Statements"* (1974, Computing Surveys): formal analysis showing `goto` for cleanup is a valid, structured use.
4. **Eli Bendersky** — [*"Uses of goto in C"*](https://eli.thegreenplace.net/2009/04/27/using-goto-for-error-handling-in-c): practical analysis of `goto` for resource cleanup in real C projects.
5. **Linux Device Drivers, 3rd Ed.** — O'Reilly: documents `goto` cleanup as standard practice in kernel module `init`/`exit` functions.
6. **Stack Overflow** — [*"Valid use of goto for error management in C"*](https://stackoverflow.com/questions/245742): community consensus with 500+ upvotes endorsing the pattern.
