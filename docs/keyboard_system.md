# Keyboard Help & Binding System

This document describes the interactive keyboard help overlay and the underlying `AppBindingRegistry` system.

## Overview

The help system provides a modern, interactive visualization of the application's key bindings. It consists of two main parts:

1. **`AppBindingRegistry`**: A centralized data structure that stores all key metadata (key, modifiers, category, description).
2. **Help Overlay UI**: A dynamic keyboard layout renderer that uses the registry to highlight active keys and show their functionality.

## Architecture

### Centralized Registry

All bindings are defined in [app_binding.c](src/app_binding.c). The registry is owned by the main `App` struct and initialized during startup.

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

1. Open [src/app_binding.c](src/app_binding.c).
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

The keyboard layout is defined in `src/app_ui.c` and is centered automatically. You can adjust the visual parameters (size, padding, glassmorphism intensity) via the `KeyboardLayoutConfig` in `app_init`.

The key positions and labels are stored in the `KEY_LAYOUT_QWERTY` array within `app_ui.c`. Each entry defines:

- The `key` handled.
- The `row` index.
- The `xPos` relative to the row start.
- The `width` of the key (unit: square key width).
- The `label` displayed on the key cap.
