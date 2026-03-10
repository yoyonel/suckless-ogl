# Performance Mode & Notifications

This document details the architecture and usage of the **Performance Mode** optimization subsystem and the **Action Notification** UI feedback system.

## Performance Mode

The `PerfMode` module provides a unified interface for activating system-level performance optimizations, designed to enhance the fluidity of the application (e.g., maximizing FPS, minimizing stutter).

### Architecture

The refactored design uses a **context-based approach** (`PerfModeContext`), eliminating global variables to ensure thread safety and testability. It supports two backends:

1.  **GameMode**: Leverages Feral Interactive's `libgamemode` daemon (preferred).
2.  **Native**: Fallback using Linux scheduling syscalls (`sched_setscheduler`, `setpriority`).

```mermaid
classDiagram
    class App {
        +perf_context
        +perf_mode_active
        +init()
        +cleanup()
    }

    class PerfModeContext {
        +state
        +backend
        +original_policy
        +original_param
        +original_nice
        +initialized
    }

    class Backend
    <<interface>> Backend
    Backend : +activate()
    Backend : +deactivate()

    class GameModeBackend {
        +libgamemode_init
    }

    class NativeBackend {
        +sched_setscheduler_FIFO
        +setpriority_nice
    }

    App *-- PerfModeContext
    PerfModeContext ..> GameModeBackend : Tries_First
    PerfModeContext ..> NativeBackend : Fallback
```

### Usage Flow

1.  **Initialization**: `perf_mode_init` detects available backends and saves the current process scheduling state.
2.  **Activation**: `perf_mode_request_start` attempts to enable optimizations via the best available backend.
3.  **Deactivation**: `perf_mode_request_end` reverts changes (restoring original scheduler policy/nice values).
4.  **Cleanup**: `perf_mode_cleanup` ensures any active mode is disabled before exit.

## Action Notifications

The `ActionNotifier` system provides immediate visual feedback for user actions (e.g., changing settings, taking screenshots) via a non-intrusive on-screen overlay.

### Design

It uses a **circular buffer** of notification slots to manage message lifetimes without dynamic allocation during runtime.

-   **Zero Allocation**: Pre-allocated array of `ActionNotification` structs.
-   **Auto-expiration**: Each notification has a `lifetime` and `max_lifetime`.
-   **FIFO Replacement**: If the buffer is full, the oldest active notification is overwritten.

```mermaid
sequenceDiagram
    participant User
    participant AppInput
    participant ActionNotifier
    participant UI

    User->>AppInput: Press Key (e.g., F9)
    AppInput->>AppInput: Toggle Feature (PerfMode)
    AppInput->>ActionNotifier: action_notifier_push("Perf Mode: ON", 2.0s)

    activate ActionNotifier
    ActionNotifier->>ActionNotifier: Find free slot / Overwrite oldest
    ActionNotifier->>ActionNotifier: Copy text (safe_strncpy)
    ActionNotifier-->>AppInput: Done
    deactivate ActionNotifier

    loop Every Frame
        AppInput->>ActionNotifier: action_notifier_update(dt)
        ActionNotifier->>ActionNotifier: Increase lifetime, deactivate if expired
        AppInput->>ActionNotifier: action_notifier_draw(ui_ctx)
        ActionNotifier->>UI: ui_draw_text_ex(...)
    end
```

### Implementation Details

-   **Safe Strings**: Uses `safe_strncpy` to prevent buffer overflows.
-   **Alpha Blending**: Notifications fade out gracefully using the new `ui_draw_text_ex` API which supports alpha transparency.
