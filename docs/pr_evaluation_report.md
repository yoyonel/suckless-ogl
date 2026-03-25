# PR Evaluation: Shader Path Security 🛡️

This PR brings a critical security and robustness improvement to the shader loader (`@header`). It addresses not only Path Traversal vulnerabilities but also improves internal error handling (truncation).

## 1. PR Contribution

- **Security (Zero-Trust Perimeter)**: Adding `is_safe_path` prevents the use of `..`, absolute paths `/`, and suspicious characters (`:`, `\`). This is essential to prevent a malicious shader from reading sensitive files (e.g., `/etc/passwd` or SSH keys) via relative includes.
- **Buffer Robustness**: Replacement of potentially risky string operations with explicit size checks (`safe_snprintf`, `safe_memcpy`) and path truncation detection.
- **Testability**: Introduction of "Standalone" tests, enabling business logic testing without heavy dependencies (GPU, Drivers).

## 2. Code Quality

- **Defensive Programming**: Using `MAX_INCLUDE_DEPTH` to limit recursion and `MAX_SHADER_SOURCE_SIZE` (16MB) to prevent denial-of-service (DoS) attacks.
- **RAII Management**: Smart use of `__attribute__((cleanup(ctx_free)))` to guarantee memory release for the complex include system, even on early error.
- **Parsing Precision**: The `@header` parser correctly handles paths with or without quotes, and strips trailing spaces/carriage returns.

## 3. Analysis of `tests/test_shader_path_security_standalone.c`

### Test Method: "Mocking & Injection"

The file uses a clever strategy to isolate the logic from `src/shader.c`:

1. **Lightweight Mocks**: Instead of linking `glad` or the complex logging system, the test redefines "empty" functions (lines 11–173). This allows the compiler to satisfy symbols without OpenGL infrastructure.
2. **Direct C Include**: `#include "shader.c"` (line 175). This is the key to testing `static` functions (e.g., `get_dir_from_path`, `parse_include_path`) without exposing them in the public header.
3. **Total Isolation**: The test compiles to a minimal, ultra-fast, deterministic executable — ideal for CI running without a GPU (e.g., standard GitHub Actions).

### Security Focus via Mock

The test specifically validates **truncation edge cases**. For example, `test_get_dir_from_path_truncation` verifies that if the output buffer is too small to hold the directory, the function fails cleanly (`false`) instead of copying a potentially dangerous or invalid truncated string.

---

## Conclusion & Recommendations

**Score: 9/10**

- **Strengths**: Granular security, fast tests, clean code.
- **Possible improvement**: Add a test in the standalone suite for `is_safe_path` itself with a list of classic "payloads" (`../../`, `/etc/`, `C:\`, etc.) to centralize blacklist validation.

> [!TIP]
> The method of directly including the `.c` file in tests is excellent for "infrastructure" code like this. It allows achieving 100% branch coverage on parsing logic without needing to create physical files on disk for each combination.
