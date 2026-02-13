# OpenGL State Integrity Monitor Report

## Detected Inconsistencies

### 1. UI Context Re-initialization Leak
* **Target:** `src/ui.c:235` (in `ui_init`)
* **Description:** The `ui_init` function unconditionally overwrites the `UIContext` structure members (`texture`, `vao`, `vbo`, `shader`) without checking if they are already allocated. If `ui_init` is called on an already initialized context, the existing OpenGL resources (texture, VAO, VBO, shader programs) are leaked as their handles are lost.
* **Confidence Score:** 100%
* **Mesa Trace:** N/A (Static Analysis)
* **Remediation:**
```c
int ui_init(UIContext* ui_context, const char* font_path, float font_size)
{
    if (ui_context == NULL || font_path == NULL) {
        LOG_ERROR("ui", "Invalid arguments to ui_init");
        return 0;
    }

    /* Check if already initialized to prevent leaks */
    if (ui_context->texture != 0 || ui_context->vao != 0) {
        LOG_WARNING("ui", "UI Context re-initialization detected. Cleaning up old resources.");
        ui_destroy(ui_context);
    }

    // Initialize to safe defaults...
    // ...
}
```

### 2. GPU Profiler Re-initialization Leak
* **Target:** `src/gpu_profiler.c:19` (in `gpu_profiler_init`)
* **Description:** The `gpu_profiler_init` function resets the `GPUProfiler` structure using compound literal assignment `*profiler = (GPUProfiler){0};`. This overwrites any existing query handles stored in `profiler->buffers`. If the profiler was already initialized, the OpenGL query objects are leaked.
* **Confidence Score:** 100%
* **Mesa Trace:** N/A (Static Analysis)
* **Remediation:**
```c
void gpu_profiler_init(GPUProfiler* profiler)
{
    if (!profiler) {
        return;
    }

    /* Check if already initialized */
    if (profiler->buffers[0].queries[0].query_start != 0) {
        LOG_WARNING("gpu_profiler", "Profiler re-initialization detected. Cleaning up.");
        gpu_profiler_cleanup(profiler);
    }

    /* 1. Safe Zero Initialization */
    *profiler = (GPUProfiler){0};
    // ...
}
```

### 3. App Structure Re-initialization Leak
* **Target:** `src/app.c:42` (in `app_init`)
* **Description:** The `app_init` function uses `memset(app, 0, sizeof(App))` to clear the application structure. This blindly overwrites all OpenGL resource handles (textures, buffers, shaders, window context) stored in the structure. If `app_init` is called on an already initialized `App` instance (e.g., during a soft restart), all resources are leaked.
* **Confidence Score:** 100%
* **Mesa Trace:** N/A (Static Analysis)
* **Remediation:**
```c
int app_init(App* app, int width, int height, const char* title)
{
    /* Check for existing window or resources */
    if (app->window != NULL) {
        LOG_WARNING("app", "App re-initialization detected. Cleaning up.");
        app_cleanup(app);
    }

    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)memset(app, 0, sizeof(App));
    // ...
}
```

### 4. Skybox Re-initialization Leak
* **Target:** `src/skybox.c:19` (in `skybox_init`)
* **Description:** The `skybox_init` function calls `render_utils_create_fullscreen_quad`, which generates new VAO and VBO handles and assigns them to `skybox->vao` and `skybox->vbo`. It does not check if these members already hold valid handles. If called on an initialized `Skybox` struct, the old VAO and VBO are leaked.
* **Confidence Score:** 100%
* **Mesa Trace:** N/A (Static Analysis)
* **Remediation:**
```c
void skybox_init(Skybox* skybox, Shader* shader)
{
    if (skybox->vao != 0) {
        skybox_cleanup(skybox);
    }

    /* Cache uniform locations... */
    // ...
}
```

### 5. Debug Callback Suppression
* **Target:** `src/gl_debug.c:110` (in `gl_debug_callback`)
* **Description:** The `glDebugMessageCallback` implementation includes logic to suppress repeated messages by checking `if (entry->count == 1)`. While this prevents log flooding, it might hide the frequency of errors or intermittent issues that occur multiple times. For high-sensitivity monitoring, it is preferable to log all occurrences or at least provide a summary count.
* **Confidence Score:** 80%
* **Mesa Trace:** N/A
* **Remediation:**
```c
/* Suggestion: Add a toggle or counter logging */
static void APIENTRY gl_debug_callback(...)
{
    // ...
    entry->count++;

    if (entry->count == 1 || (entry->count % 100 == 0)) {
        // Log every 100th occurrence to indicate persistence
        // ...
    }
}
```

## Debug Context Verification
The application correctly requests a debug context in `src/window.c`:
```c
glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
```
And enables it with `GL_DEBUG_OUTPUT_SYNCHRONOUS` in `src/gl_debug.c`, ensuring that errors are reported at the point of failure. This configuration is compliant with high-sensitivity debugging requirements.
