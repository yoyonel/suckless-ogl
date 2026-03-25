# Standalone Unit Testing & Mocking Strategy 🧪

This document describes the standalone testing and mocking technique used in the `suckless-ogl` project, particularly for validating complex business logic (such as shader parsing) without GPU dependencies.

## 1. Philosophy

The goal is to test a unit of code (a `.c` file) in complete isolation. This enables:

- Instant execution (< 10ms).
- No need for a valid OpenGL context or GPU drivers.
- Perfect compatibility with restricted CI environments.

## 2. Compile-Time "Shadowing" Mocking

Unlike heavy mocking frameworks that rely on runtime dependency injection, we use a compile-time approach:

### A. Symbol Redefinition

In the test file (e.g., `tests/test_shader_path_security_standalone.c`), we redefine the external library functions (GLAD, Log) that the tested code depends on.

```c
/* Mock OpenGL Definition */
GLuint glCreateShader(GLenum type) {
    (void)type;
    return 1; // Returns a fake ID
}
```

### B. Direct Implementation Include

To test private (`static`) functions without exposing them in the public `.h` header, the test file directly includes the `.c` file after defining the mocks.

```c
/* 1. Mocks */
void log_message(...) { /* no-op */ }

/* 2. Target Implementation */
#include "shader.c"

/* 3. Tests */
void test_logic() {
    // Calling a static function defined in shader.c
    ASSERT_TRUE(get_dir_from_path(...));
}
```

## 3. Advantages and Limitations

| Advantage | Description |
| :--- | :--- |
| **Static Access** | Allows testing 100% of internal logic without API pollution. |
| **Speed** | No heavy dynamic linking or hardware initialization. |
| **Zero Dependency** | The test binary is "pure C". |

| Limitation | Precaution |
| :--- | :--- |
| **Symbol Collision** | Cannot be linked with the real library at the same time. |
| **Maintenance** | If the real API changes, the mock must be manually synchronized. |

## 4. Concrete Example: Security Standalone

The test `tests/test_shader_path_security_standalone.c` perfectly demonstrates this approach:

1. It defines stubs for ~20 OpenGL functions.
2. It includes `src/shader.c`.
3. It validates the `is_safe_path` algorithms and path buffer management.

> [!TIP]
> Use this approach for any parsing logic, mathematical computation, or resource management (RAII) that does not require actual hardware reads.
