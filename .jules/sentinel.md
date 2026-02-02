## 2026-02-01 - [DoS] Unbounded File Read in Shader Loading
**Vulnerability:** `load_file_into_ram` in `src/shader.c` did not enforce `MAX_SHADER_SOURCE_SIZE` (16MB), allowing arbitrary allocation size based on file size, leading to potential DoS via OOM.
**Learning:** Even when limits are defined (`MAX_SHADER_SOURCE_SIZE` existed as an enum), they must be explicitly enforced in the code. The constant was previously unused.
**Prevention:** Always validate input sizes against defined limits before allocation. Ensure defined constants are actually used in logic.

## 2026-02-02 - [DoS] Unbounded Texture Dimensions
**Vulnerability:** Texture loading and upload functions did not validate dimensions against a safe limit, allowing massive allocations or buffer over-reads via crafted images or API misuse.
**Learning:** GL driver limits are not a security feature. `glTexStorage2D` might succeed for large values that crash `glTexSubImage2D` if source buffer is too small, or simply exhaust VRAM.
**Prevention:** Define explicit application-level limits (e.g., `MAX_TEXTURE_DIMENSION`) and enforce them before passing data to OpenGL or allocators.
