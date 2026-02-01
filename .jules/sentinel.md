## 2026-02-01 - [DoS] Unbounded File Read in Shader Loading
**Vulnerability:** `load_file_into_ram` in `src/shader.c` did not enforce `MAX_SHADER_SOURCE_SIZE` (16MB), allowing arbitrary allocation size based on file size, leading to potential DoS via OOM.
**Learning:** Even when limits are defined (`MAX_SHADER_SOURCE_SIZE` existed as an enum), they must be explicitly enforced in the code. The constant was previously unused.
**Prevention:** Always validate input sizes against defined limits before allocation. Ensure defined constants are actually used in logic.
