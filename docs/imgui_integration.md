# Dear ImGui Integration Architecture

This document describes the design, architecture, and integration of **Dear ImGui (v1.92.4)** in the C11 `suckless-ogl` engine.

## Overview

Dear ImGui provides a real-time, interactive graphical user interface (GUI) overlay for runtime diagnostics, parameter tuning, profiling, and scene inspection. It is toggled using the **`F2`** key.

```
       [ GLFW Input Callback ]
                  │ (F2 Key / Mouse / Scroll Events)
                  ▼
         [ App Input Layer ]
                  │
                  │ (Delegates to App Coordinator)
                  ▼
         [ App Coordinator ] ──► [ Sync Cursor Capture & Camera Status ]
                  │
                  ▼
          [ ImGui Subsystem ] ──► [ Render Layout Panels & Pixel Inspector ]
```

---

## Architectural Principles (SRP & SoC)

To maintain a clean codebase, the ImGui integration strictly adheres to the **Single-Responsibility Principle (SRP)** and **Separation of Concerns (SoC)**:

1. **Input Callback Decoupling (`src/app_input.c`)**:
   * The input system is only responsible for catching raw GLFW keys and forwarding them.
   * When `F2` is pressed, the input callback calls `app_toggle_gui(app)`. It does *not* modify cursor modes, camera structures, or ImGui visibility states.

2. **State Synchronization Coordinator (`src/app.c`)**:
   * The functions `app_set_gui_visible` and `app_toggle_gui` synchronize the engine state.
   * When the GUI is shown, it automatically releases mouse capture (`GLFW_CURSOR_NORMAL`), disables camera movements, and logs the change.
   * When hidden, it captures the mouse cursor (`GLFW_CURSOR_DISABLED`), enables camera movement, and resets mouse smoothing state.

3. **ImGui Layout & Rendering Bridge (`src/gui.h` / `src/gui.cpp`)**:
   * Encapsulated behind a C-compatible bridge interface (`extern "C"`).
   * Renders controls, updates parameters, and provides texture debug views without exposing C++ types to the rest of the C compiler unit.

---

## Layout & Panels

The interface consists of the following tab panels:

* **Camera**: Adjust move speeds, sensitivity, FOV, and head-bobbing configurations.
* **Scene**: Toggle skybox visibility, adjust blur levels, and select rendering/sorting modes.
* **Rendering**: Monitor active MSAA sample count and tune Specular Anti-Aliasing parameters.
* **Post-FX**: Direct control over Vignette, Exposure, Chromatic Aberration, Color Grading, Bloom, FXAA, Auto-Exposure, Depth of Field, and Fog.
* **Profiling**: Displays CPU/GPU execution times in a tabular format.
* **Shaders**: Monitor compiled shader variants and optimization cache statuses.
* **IBL Debug**: Live visualization of the active environment HDR maps, Irradiance Map, Prefiltered Specular Map (with mip level controls), and BRDF LUT.
* **Compute Slicing**: Controls Progressive IBL sample budgets and frame-sliced convolving limits.

---

## Click-to-Inspect Pixel Inspector

The interface includes a **Pixel Inspector** mode:
1. Double-clicking on any preview texture (e.g., BRDF LUT, Irradiance) opens a zoom view.
2. Clicking on a pixel within the preview samples the texture color in real-time.
3. Behind the scenes, a temporary Framebuffer Object (FBO) is bound, and `glReadPixels` is executed to retrieve exact floating-point color vectors (`RGBA32F`) from the GPU memory.

---

## Diagnostics & Logging

All ImGui lifecycle events are trace-logged under the `"suckless-ogl.gui"` module:
* **Startup**: Traces initialization of GLFW/OpenGL3 backends.
* **Activation**: Logs cursor capture state transitions and camera control toggles.
* **Shutdown**: Details resources release and context destruction.
