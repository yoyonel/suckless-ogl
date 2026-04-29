# Include-What-You-Use (IWYU) Audit

## Overview

[Include-What-You-Use](https://include-what-you-use.org/) (IWYU) is a static
analysis tool that ensures every C/C++ source file includes exactly what it
uses — no more, no less. It helps reduce compilation times, prevent transitive
dependency coupling, and keep the include graph clean.

This project uses IWYU as part of the dependency hygiene strategy (see
[Architecture](architecture.md) for the include-dependency graph).

## Installation

```bash
# Debian/Ubuntu
sudo apt install -y iwyu

# Or via nala
sudo nala install -y iwyu

# Verify
iwyu --version
# Expected: include-what-you-use 0.23 based on clang ...
```

IWYU requires a **Clang-compatible compilation database** (`compile_commands.json`).
The project generates one automatically via CMake in the `build/` directory.

## Running the Audit

### Full Project Scan

```bash
# Run IWYU on all translation units using the compile database
iwyu_tool -p build 2>&1 > /tmp/iwyu_full.txt

# Without forward-declaration suggestions (recommended for C projects)
iwyu_tool -p build -- -Xiwyu --no_fwd_decls 2>&1 > /tmp/iwyu_full.txt
```

### Single File

```bash
# Analyze one file with its compile flags from the database
iwyu_tool -p build -- -Xiwyu --no_fwd_decls src/app.c 2>&1
```

### Filtering Results

```bash
# Show only files with suggested changes (skip "has correct #includes")
grep -E "should (add|remove)" /tmp/iwyu_full.txt

# Count total removable includes
grep "^- #include" /tmp/iwyu_full.txt | wc -l

# Extract per-file removal summary (project headers only)
grep -A100 "should remove" /tmp/iwyu_full.txt \
  | grep '^- #include "' \
  | sort | uniq -c | sort -rn
```

## Interpreting Results

IWYU output follows this format for each translation unit:

```text
path/to/file.c should add these lines:
#include <stdint.h>      // uint32_t
#include "other.h"       // SomeType

path/to/file.c should remove these lines:
- #include <stdio.h>     // lines 5-5
- #include "unused.h"    // lines 8-8

The full include list for path/to/file.c:
#include "file.h"
#include <stdint.h>
#include "other.h"
```

### Sections

| Section | Meaning |
|---------|---------|
| **should add** | Headers that the file uses directly but doesn't include |
| **should remove** | Headers that are included but not directly used |
| **full include list** | What IWYU thinks the correct set of includes should be |
| **has correct #includes** | File needs no changes |

### Common False Positives

IWYU is powerful but imperfect, especially for C projects with macros and
conditional compilation. Always **verify before applying** suggestions:

| Pattern | Why It's a False Positive |
|---------|--------------------------|
| `gl_common.h` → `glad/glad.h` | `gl_common.h` provides RAII macros (`GL_SAFE_DELETE_*`, `GL_SCOPE_*`) beyond just GL types. Replacing it breaks the API contract. |
| `immintrin.h` → `emmintrin.h` + `smmintrin.h` | `immintrin.h` is the umbrella header for SSE/AVX/F16C. Splitting it misses AVX-256 intrinsics. |
| `sched.h` removed from header | Header defines `struct sched_param` used in a struct member — IWYU sometimes misses struct field dependencies. |
| System headers removed from `.h` | The `.c` file uses the types (e.g., `va_list`, `uintptr_t`) that come transitively. Moving the include to `.c` is correct; removing it entirely is not. |
| `app_settings.h` removed | May cause cascading failures — downstream files rely on transitive constants (e.g., `DEFAULT_EXPOSURE_STEP`). Fix: add direct includes in consumers. |

### Recommended Workflow

1. **Run IWYU** and save full output
2. **Categorize** removals:
   - **System headers** (`stdio.h`, `string.h`, etc.) — usually safe, but test each
   - **Project headers** (`app_settings.h`, `shader.h`) — verify symbol usage first
   - **`gl_common.h` replacements** — defer to dedicated header-fanout refactoring
3. **Apply** removals in batches, building after each batch
4. **Fix cascading issues**: when removing a transitive include from a header, consumers may need a direct include added
5. **Run full test suite** sequentially (`ctest --test-dir build --output-on-failure`)
6. **Verify** format + lint are clean

## Project Audit Results (April 2026)

The initial IWYU audit of suckless-ogl identified:

| Category | Count | Status |
|----------|------:|--------|
| System header removals | 26 | Applied (3 were false positives) |
| Project header removals | 8 | Applied |
| `gl_common.h` → `glad/glad.h` | 33 | Deferred (needs header-fanout refactoring) |
| False positives caught | 4 | `sched.h`, `immintrin.h`, `stdarg.h`+`stdint.h` in `utils.h` |
| Cascading fixes needed | 2 | `src/utils.c` (+`stdarg.h`/`stdint.h`), `src/postprocess_input.c` (+`app_settings.h`) |
| **Net includes removed** | **35** | Across 29 files |

### False Positive Rate

Out of 41 attempted removals, 4 were false positives (**~10%**). This confirms
IWYU suggestions should always be treated as advisory, not authoritative.

## Integration with CI

IWYU is not (yet) enforced in CI. The current strategy is periodic manual
audits. See issue [#266](https://github.com/yoyonel/suckless-ogl/issues/266)
for the planned CI dependency metrics gate.

## Related

- [Architecture — Include Dependency Graph](architecture.md)
- [Linting Strategy](linting_strategy.md)
- [Issue #261 — IWYU audit](https://github.com/yoyonel/suckless-ogl/issues/261)
- [Issue #266 — CI dependency metrics gate](https://github.com/yoyonel/suckless-ogl/issues/266)
