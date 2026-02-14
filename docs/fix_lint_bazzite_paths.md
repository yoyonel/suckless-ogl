# Fix: Lint Path Resolution on Bazzite Linux

**Date:** 2026-02-17
**Files:** `scripts/lint_incremental.py`
**Symptom:** `just lint` fails with `Permission denied` or silently skips all files

## Background

Bazzite (Fedora Atomic) uses `/var/home` as the physical home directory, with
`/home` as a symlink:

```
/home -> var/home
```

This creates a path identity mismatch:
- **Python / filesystem:** `os.path.abspath()` returns `/var/home/latty/...`
- **CMake:** Writes `/home/latty/...` in `compile_commands.json`

## Problem 1: Lint Cache `Permission denied`

`lint_incremental.py` computed cache paths using `os.path.relpath(file_path, os.getcwd())`.
When `file_path` was an absolute `/var/home/...` path and CWD resolved differently,
`relpath` produced traversals like `../../../../var/home/...`, causing
`os.makedirs()` to attempt creating directories outside the project.

**Fix:** Changed to `os.path.relpath(file_path, src_root)` which always produces
clean relative paths like `src/foo.c`.

## Problem 2: `run-clang-tidy` Finds 0 Files

`run-clang-tidy` uses **string matching** against `compile_commands.json` entries.
When the script passed `/var/home/latty/.../src/app.c` but the database contained
`/home/latty/.../src/app.c`, it matched 0 files and exited successfully without
linting anything.

**Fix:** Added `detect_path_prefix()` which reads the first entry from
`compile_commands.json` and compares it with the filesystem path. If a prefix
mismatch is detected (e.g., `/var/home/latty/Prog/proj` vs `/home/latty/Prog/proj`),
`normalize_path_for_clang_tidy()` remaps the prefix before passing paths to
`run-clang-tidy`.

## Backward Compatibility

On systems where `/home` is **not** a symlink, `detect_path_prefix()` returns
`(None, None)` and the normalization function is a no-op. No behavior change.
