## 2026-02-01 - [DoS] Unbounded File Read in Shader Loading
**Vulnerability:** `load_file_into_ram` in `src/shader.c` did not enforce `MAX_SHADER_SOURCE_SIZE` (16MB), allowing arbitrary allocation size based on file size, leading to potential DoS via OOM.
**Learning:** Even when limits are defined (`MAX_SHADER_SOURCE_SIZE` existed as an enum), they must be explicitly enforced in the code. The constant was previously unused.
**Prevention:** Always validate input sizes against defined limits before allocation. Ensure defined constants are actually used in logic.

## 2026-02-02 - [DoS] Unbounded Texture Dimensions
**Vulnerability:** Texture loading and upload functions did not validate dimensions against a safe limit, allowing massive allocations or buffer over-reads via crafted images or API misuse.
**Learning:** GL driver limits are not a security feature. `glTexStorage2D` might succeed for large values that crash `glTexSubImage2D` if source buffer is too small, or simply exhaust VRAM.
**Prevention:** Define explicit application-level limits (e.g., `MAX_TEXTURE_DIMENSION`) and enforce them before passing data to OpenGL or allocators.

## 2026-02-03 - [Enhancement] Missing Compiler Hardening Flags
**Vulnerability:** The build configuration lacked standard security hardening flags (Stack Protector, Fortify Source, RELRO), making the binary easier to exploit if memory corruption vulnerabilities were present.
**Learning:** Default CMake configurations often optimize for compatibility/speed rather than security. Defensive coding in source (e.g., `safe_snprintf`) is good but should be backed by compiler-level protections (Defense in Depth).
**Prevention:** Explicitly enable hardening flags (`-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `-Wl,-z,relro,-z,now`) in `CMakeLists.txt` for all production targets.

## 2026-02-04 - [Path Traversal] Shader Include Path Traversal
**Vulnerability:** The `@header` directive in shaders allowed including files via relative paths containing `..`, enabling reading of arbitrary files (limited by process permissions) into the shader source.
**Learning:** File inclusion mechanisms (like `@header`) must always sanitize input paths to prevent directory traversal. Concatenating paths is not enough; one must check for `..` or use path canonicalization.
**Prevention:** Implemented `is_safe_path` to reject `..`, absolute paths, and drive letters in included paths.

## 2026-02-05 - [Enhancement] Enforce Path Length Limits
**Vulnerability:** Ignored return values of `safe_snprintf` in `async_loader.c` and `app_env.c` could lead to silent path truncation and potential logic errors or loading of incorrect files.
**Learning:** Safe wrappers like `safe_snprintf` are only safe if their return values (indicating success/failure/truncation) are actually checked.
**Prevention:** Always check the return value of string manipulation functions. If a path is truncated, treat it as a hard error/failure rather than proceeding.

## 2026-02-06 - [Enhancement] Compile-Time Format String Validation
**Vulnerability:** `log_message` accepted a format string and arguments but lacked the compiler attribute to validate them, creating a risk of format string vulnerabilities if developers made a mistake.
**Learning:** `__attribute__((format(printf, X, Y)))` is a zero-cost, high-value defense that catches format string mismatches at compile time. It should be standard for all variadic formatting functions.
**Prevention:** Added `__attribute__((format(printf, 3, 4)))` to `log_message` in `include/log.h`.

## 2026-02-07 - [DoS] Unbounded Texture Allocation (TOCTOU)
**Vulnerability:** `texture_load` verified dimensions with `stbi_info` but then called `stbi_load` and used the returned dimensions blindly to allocate VRAM, allowing a TOCTOU or library inconsistency to bypass limits.
**Learning:** Never trust that a "load" function returns the same metadata as an "info" function called previously. Always validate data *after* loading before using it for resource allocation.
**Prevention:** Added explicit checks for `MAX_TEXTURE_DIMENSION` immediately after `stbi_load` and `stbi_loadf`, freeing the buffer if invalid.
