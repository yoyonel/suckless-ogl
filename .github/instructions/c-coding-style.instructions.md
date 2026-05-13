---
description: "Use when writing or reviewing C code. Covers type conventions, boolean usage, and patterns required by clang-tidy readability checks."
applyTo: ["src/**/*.c", "include/**/*.h", "tests/**/*.c"]
---
# C Coding Style Conventions

## Boolean Fields & Variables

### Rule: Always use C99 `bool`/`true`/`false` for boolean semantics

Any struct field, local variable, or parameter that represents a binary on/off, yes/no, or enabled/disabled state **MUST** use `bool` (from `<stdbool.h>`), not `int`.

```c
// ✅ Correct
#include <stdbool.h>
typedef struct {
    bool enabled;
    bool visible;
} Widget;

// ❌ Forbidden — int-as-boolean
typedef struct {
    int enabled;   // 0 or 1 — use bool instead
    int visible;
} Widget;
```

### Rule: Include `<stdbool.h>` in every header that declares `bool` fields

Do not rely on transitive includes. Each header is self-contained.

### clang-tidy `readability-implicit-bool-conversion` Patterns

The project enforces this check as an error. These patterns are required:

#### 1. Bool negation assignment

`!bool_expr` returns `int` in C. Assigning it back to `bool` triggers the warning.

```c
// ❌ Triggers warning
field = !field;

// ✅ Correct
field = ((!field) != 0);
```

#### 2. Bool in ternary operator condition

A `bool` used as ternary condition is implicitly converted to `int`.

```c
// ❌ Triggers warning
const char* s = flag ? "ON" : "OFF";

// ✅ Correct
const char* s = (int)flag ? "ON" : "OFF";
```

#### 3. Bool in arithmetic expressions

Bool operands in arithmetic (`+`, `-`, `*`) require explicit `(int)` cast.

```c
// ❌ Triggers warning
float input = (float)(move_right - move_left);

// ✅ Correct (move_right/move_left are bool)
float input = (float)((int)move_right - (int)move_left);
```

#### 4. Logical OR/AND assigned to bool

`||` and `&&` return `int` in C. Assignment to `bool` needs `!= 0`.

```c
// ❌ Triggers warning
bool active = a || b || c;

// ✅ Correct
bool active = (a || b || c) != 0;
```

#### 5. External API int→bool

Functions returning `int` as boolean (GLFW, OpenGL) need explicit comparison.

```c
// ❌ Triggers warning
state->connected = glfwJoystickIsGamepad(id);

// ✅ Correct
state->connected = (glfwJoystickIsGamepad(id) != 0);
```

#### 6. Bool→int for GPU uniforms/UBOs

GPU memory layout uses `int32_t`/`GLint`. Explicit cast required.

```c
// ❌ Triggers warning
glUniform1i(loc, config.wireframe);
ubo.field = source_bool;

// ✅ Correct
glUniform1i(loc, (GLint)config.wireframe);
ubo.field = (int32_t)source_bool;
```

#### 7. Compound literal initialization

`{0}` for a struct with `bool` fields triggers int→bool. Use designated init.

```c
// ❌ Triggers warning
*mgr = (Manager){0};

// ✅ Correct
*mgr = (Manager){.bool_field = false};
```

### Exceptions — Fields That Must Stay `int`

- **GPU UBO/SSBO structs** (`scene_uniforms.h`): Fields mapped to GPU memory layout stay `int32_t` — GPU has no `bool` type
- **Multi-value state** (e.g. `loading_step = 0,1,2,3`): Not boolean, keep `int`
- **Bitfields**: Use `unsigned int field : 1` when packing, not `bool`

### Helper Macros (`include/bool_utils.h`)

Instead of writing raw casts everywhere, **use the centralized macros** from `bool_utils.h`:

```c
#include "bool_utils.h"

// Toggle a bool field (replaces `x = ((!x) != 0)`)
BOOL_TOGGLE(field);

// Convert bool→int for ternary, arithmetic, or GPU uniforms
BOOL_TO_INT(flag) ? "ON" : "OFF";
float input = (float)(BOOL_TO_INT(right) - BOOL_TO_INT(left));
glUniform1i(loc, BOOL_TO_INT(config.wireframe));
ubo.field = BOOL_TO_INT(source_bool);

// Convert int→bool (external API or logical expression)
state->connected = INT_TO_BOOL(glfwJoystickIsGamepad(id));
bool active = INT_TO_BOOL(a || b || c);
```

**Prefer macros** over raw casts in new code. They centralize the workaround and can be updated if C23 or future clang-tidy changes remove the need.

## Volatile Booleans (Threading)

For boolean flags shared across threads, use `volatile bool`:

```c
volatile bool running;
volatile bool results_ready;
```

This is well-defined in C99/C11 for simple flag signaling (not for synchronization — use mutexes for that).
