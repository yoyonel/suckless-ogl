# Keyboard Help & Binding System

This document describes the interactive keyboard help overlay and the underlying `AppBindingRegistry` system.

## Overview

The help system provides a modern, interactive visualization of the application's key bindings. It consists of two main parts:

1. **`AppBindingRegistry`**: A centralized data structure that stores all key metadata (key, modifiers, category, description).
2. **Help Overlay UI**: A dynamic keyboard layout renderer that uses the registry to highlight active keys and show their functionality. It features a Cyberpunk aesthetic using pre-rendered PNGs and procedural SDF (Signed Distance Field) effects.

## Visual Design & Polish

The help overlay utilizes an advanced hybrid rendering approach defined in `src/app_ui.c` and `shaders/ui.frag`:

1. **Cyberpunk Textures**: The base panel UI frame and individual keys are rendered using high-quality PNG textures (`kbd_panel_frame.png` and `kbd_key_base.png`). Unbound keys sit quietly in the background, while bound keys are tinted via shader according to their binding type constraint (Toggle, Cycle, Combo).
2. **Procedural Neon Bloom**: When a key is pressed, an intense, procedural SDF neon border glow is drawn directly by `ui.frag` (Mode `5.0`). The intensity of the glow follows a mathematically precise exponential decay for a crisp, luminous look.
3. **Responsive UI Scaling**: The entire overlay scales dynamically based on the window height. A `ui_scale` factor is calculated relative to a reference resolution (1080p), ensuring the overlay remains perfectly proportioned and legible on any screen size (from 720p to 4K).
4. **Scalable Text Rendering**: Uses `ui_draw_text_scaled()` to render Freetype glyphs with on-the-fly metric scaling (width, height, advance, and offsets). This allows text to scale perfectly with the UI shapes without requiring multiple font rasterization passes.
5. **Spotlight Dimming & Hover Decay**: Activating or hovering over a key smoothly dims the rest of the keyboard. To prevent flickering when the mouse passes over gaps between keys, a **150ms Hover Decay** timer maintains the dimmed state for a smooth visual experience.
6. **Smart Modifier Highlighting**: The overlay actively queries the binding registry to determine whether a combination of keys relies on a given modifier (e.g. `Shift`). Pressing physical modifier keys alone, or paired carelessly with non-modifier keys, won't artificially light up the Shift key unless the active function strictly demands it.

## Architecture

### Centralized Registry

All bindings are defined in `src/app_binding.c`. The registry is owned by the main `App` struct and initialized during startup.

```c
typedef struct {
    int key;                // GLFW_KEY_* constant
    int mods;               // GLFW_MOD_* bitmask
    const char* action;      // Short title (e.g., "Toggle Bloom")
    const char* desc;        // Detailed description
    BindingCategory category;// Movement, Visuals, PostFX, System
    BindingType type;        // Toggle, Cycle, Action
} AppBinding;
```

### The "Dry-Run" Capture

When the help overlay is visible (`app->show_help == true`), the `key_callback` in `app_input.c` enters a **Dry-Run** mode:

- It intercepts key presses.
- It updates `app->help_pressed_key` and `app->help_pressed_mods`.
- It **prevents** the actual command from executing, allowing the user to explore the keyboard safely.

## Maintaining Bindings

### Adding a New Binding

To add a new key binding to the help system:

1. Open `src/app_binding.c`.
2. Locate the `app_binding_registry_init` function.
3. Use the `add_binding` helper macro:

```c
add_binding(GLFW_KEY_X, 0, "My Action", "Extra detail about what X does.",
            BINDING_CAT_POSTFX, BINDING_TYPE_TOGGLE);
```

### Modifiers Support

The system supports key combinations (e.g., `Shift + Key`). To add one:

```c
add_binding(GLFW_KEY_Y, GLFW_MOD_SHIFT, "Toggle Probes", "...",
            BINDING_CAT_VISUALS, BINDING_TYPE_TOGGLE);
```

### Categories & Colors

The UI automatically color-codes keys based on their `BindingType`:

- **Cyan (`BINDING_TYPE_TOGGLE`)**: On/Off switches.
- **Green (`BINDING_TYPE_CYCLE`)**: Actions that cycle through multiple states.
- **Orange/Yellow (`GLFW_MOD_SHIFT`)**: Combination keys.
- **Blue (`BINDING_TYPE_ACTION`)**: One-shot actions (Reset, Screenshot, Exit).

## UI Customization

The keyboard layout is defined in `include/app_ui.h` and is centered automatically. You can adjust the visual parameters (size, padding, textures) via the `KeyboardLayoutConfig` and associated constants in `include/app_ui.h`.

The key positions and labels are stored in the `KEY_LAYOUT_QWERTY` array within `include/app_ui.h`. Each entry defines:

- The `key` handled.
- The `row` index.
- The `xPos` relative to the row start.
- The `width` of the key (unit: square key width).
- The `label` displayed on the key cap.
