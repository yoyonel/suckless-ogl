# Automatic Resource Management (RAII) in C

This document explains the **RAII-style** (Resource Acquisition Is Initialization) approach used in this project to manage performance timers and other resources automatically, leveraging the `__attribute__((cleanup))` compiler extension.

---

## 1. The Concept: RAII in C

In standard C, resources (memory, file handles, timers) must be managed manually. This often leads to:
1.  **Deep indentation** when using scope-based macros (like `for` loops).
2.  **Resource leaks** when a function returns early due to an error.
3.  **Repetitive code** (multiple `stop_timer()` or `free()` calls before every `return`).

To solve this, we use a technique borrowed from C++ called RAII, made possible in C by a GCC/Clang extension.

### The `__attribute__((cleanup))` Extension
This attribute tells the compiler to automatically call a specific "cleanup function" when a local variable goes out of scope.

---

## 2. Implementation in this Project

We use this primarily for performance monitoring via the `HYBRID_FUNC_TIMER` macro defined in `include/perf_timer.h`.

### The Core Components

1.  **The RAII Container**: A structure that holds the resource and its metadata.
    ```c
    typedef struct {
        HybridTimer timer;
        const char* label;
    } HybridTimerRAII;
    ```

2.  **The Cleanup Function**: A static function that the compiler will trigger.
    ```c
    static inline void hybrid_timer_cleanup_raii(HybridTimerRAII* timer_raii) {
        perf_hybrid_stop(&timer_raii->timer, timer_raii->label);
    }
    ```

3.  **The Macro**: A convenient way to declare the guarded variable.
    ```c
    #define HYBRID_FUNC_TIMER(label) \
        HybridTimerRAII _h_raii __attribute__((cleanup(hybrid_timer_cleanup_raii))) = { \
            perf_hybrid_start(), label }
    ```

---

## 3. Benefits & Usage

### Less Indentation
Unlike the older `HYBRID_MEASURE_LOG` which required a code block `{ ... }`, the new RAII macro allows for "flat" code.

**Old Way (Indented):**
```c
void process() {
    HYBRID_MEASURE_LOG("Task") {
        do_work();
        do_more_work();
    }
}
```

**New Way (Flat):**
```c
void process() {
    HYBRID_FUNC_TIMER("Task");

    do_work();
    do_more_work();
} // Timer stops here automatically
```

### Safety with Early Returns
The cleanup is guaranteed to run even if the function exits early.

```c
void load_data(const char* path) {
    HYBRID_FUNC_TIMER("File Load");

    FILE* f = fopen(path, "r");
    if (!f) return; // Timer STOPS and LOGS here automatically!

    // ... processing ...
} // Timer STOPS and LOGS here automatically!
```

---

## 4. Real-world Example: `src/pbr.c`

In the IBL (Image Based Lighting) generation pipeline, we use `HYBRID_FUNC_TIMER` at the start of expensive compute shader dispatches.

```c
GLuint build_irradiance_map(GLuint shader, GLuint env_hdr_tex, int size, float threshold) {
    if (shader == 0) return 0;

    GLuint irr_tex = 0;
    HYBRID_FUNC_TIMER("IBL: Irradiance Map"); // Automatic measurement starts

    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "IBL: Irradiance Map");

    // ... OpenGL Setup ...
    glDispatchCompute(groups, groups, 1);
    // ...

    glPopDebugGroup();

    return irr_tex;
} // Measurement stops and result is printed to log
```

---

## 5. Compatibility & Requirements

*   **Compilers**: This feature requires **GCC** or **Clang**. It is not part of the standard ISO C (C99/C11), but it is a de-facto standard in professional Linux C programming (used extensively in the Linux Kernel and `systemd`).
*   **Order of Execution**: If multiple variables in the same scope have cleanup attributes, they are executed in **reverse order** of declaration (LIFO).
*   **Caveats**: The cleanup function is NOT called if the program terminates via `exit()` or `abort()`. It only triggers when leaving a scope normally or via `goto`, `break`, `continue`, or `return`.

---

## Further Reading
- [GCC Variable Attributes: cleanup](https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html)
- [RAII in C (Article)](https://echorand.me/posts/raii-in-c/)
