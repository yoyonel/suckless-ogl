#!/usr/bin/env python3
import os
import sys
import subprocess
import glob
import time
from concurrent.futures import ThreadPoolExecutor

# Configuration
if len(sys.argv) > 1:
    BUILD_DIR = sys.argv[1]
else:
    BUILD_DIR = "build"

src_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIRS = [os.path.join(src_root, "src")]

# Derive cache directory from build directory to allow multiple build configs
# e.g., build -> .lint_cache_build
#       .lint_full -> .lint_cache_.lint_full
build_dir_name = os.path.basename(os.path.normpath(BUILD_DIR))
CACHE_DIR = os.path.join(src_root, f".lint_cache_{build_dir_name}")

COMPILE_COMMANDS = os.path.join(BUILD_DIR, "compile_commands.json")
CLANG_TIDY_CONFIG = os.path.join(src_root, ".clang-tidy")

def detect_path_prefix():
    """Detect path prefix mismatch between filesystem and compile_commands.json.

    On systems where /home is a symlink (e.g. Bazzite: /home -> var/home),
    glob discovers files at /var/home/... but CMake writes /home/... in
    compile_commands.json. run-clang-tidy does string matching and finds 0 files.
    """
    import json
    try:
        with open(COMPILE_COMMANDS) as f:
            db = json.load(f)
        if not db:
            return None, None
        db_path = db[0].get("file", "")
        # Find the common suffix between src_root and the db path
        # e.g., src_root = /var/home/latty/Prog/proj
        #        db_path = /home/latty/Prog/proj/src/foo.c
        # We want to map /var/home -> /home
        for entry in db:
            db_file = entry.get("file", "")
            # Find a source file that should be under src_root
            rel = None
            for src_dir in SRC_DIRS:
                try:
                    rel = os.path.relpath(db_file, os.path.dirname(src_dir).replace(src_root, "").lstrip("/") or "/")
                except ValueError:
                    pass
            # Simpler: check if the filename matches
            basename = os.path.basename(db_file)
            for src_dir in SRC_DIRS:
                candidate = os.path.join(src_dir, basename)
                if os.path.exists(candidate):
                    # We have: candidate = /var/home/.../src/foo.c
                    #          db_file   = /home/.../src/foo.c
                    # Strip common suffix to find prefix mapping
                    suffix = os.path.join("src", basename)
                    if candidate.endswith(suffix) and db_file.endswith(suffix):
                        fs_prefix = candidate[:-len(suffix)].rstrip("/")
                        db_prefix = db_file[:-len(suffix)].rstrip("/")
                        if fs_prefix != db_prefix:
                            return fs_prefix, db_prefix
                    return None, None
    except (json.JSONDecodeError, OSError, IndexError):
        pass
    return None, None

# Detect path prefix mismatch (e.g., /var/home vs /home)
_FS_PREFIX, _DB_PREFIX = detect_path_prefix()

def normalize_path_for_clang_tidy(path):
    """Convert filesystem path to match compile_commands.json paths."""
    if _FS_PREFIX and _DB_PREFIX and path.startswith(_FS_PREFIX):
        return _DB_PREFIX + path[len(_FS_PREFIX):]
    return path

def get_mtime(path):
    try:
        return os.path.getmtime(path)
    except OSError:
        return 0

def run_clang_tidy(file_path):
    """Runs clang-tidy on a single file."""
    # Normalize path to match compile_commands.json (handles /var/home vs /home)
    normalized_path = normalize_path_for_clang_tidy(file_path)
    cmd = ["run-clang-tidy", "-p", BUILD_DIR, "-quiet", normalized_path]

    try:
        # Capture output.
        # run-clang-tidy outputs to stdout.
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False
        )

        # If output contains "error:", consider it a failure even if exit code is 0
        # (clang-tidy sometimes returns 0 even with errors if not strict)
        failed = result.returncode != 0 or ": error:" in result.stdout

        return not failed, result.stdout
    except Exception as e:
        return False, str(e)

def process_file(file_path):
    """Checks dependencies and runs lint if needed."""
    try:
        rel_path = os.path.relpath(file_path, src_root)
        # Cache file path: .lint_cache/src/foo.c.linted
        cache_file = os.path.join(CACHE_DIR, rel_path + ".linted")

        src_mtime = get_mtime(file_path)
        cache_mtime = get_mtime(cache_file)

        # Global dependencies (compile_commands.json, .clang-tidy)
        global_deps = [COMPILE_COMMANDS, CLANG_TIDY_CONFIG]
        global_mtime = max([get_mtime(f) for f in global_deps])

        # Incremental check
        if os.path.exists(cache_file) and cache_mtime >= src_mtime and cache_mtime >= global_mtime:
            return "SKIPPED", file_path, ""

        # Run lint
        success, output = run_clang_tidy(file_path)

        if success:
            # Update cache
            os.makedirs(os.path.dirname(cache_file), exist_ok=True)
            with open(cache_file, 'w') as f:
                f.write(f"Linted {file_path} at {time.ctime()}\n")
            return "LINTED", file_path, output
        else:
            return "FAILED", file_path, output

    except Exception as e:
        return "ERROR", file_path, str(e)

def main():
    if not os.path.exists(BUILD_DIR):
        print(f"Error: Build directory '{BUILD_DIR}' not found. Run 'just configure' first.")
        sys.exit(1)

    # Ensure cache dir exists
    if not os.path.exists(CACHE_DIR):
        os.makedirs(CACHE_DIR)

    # Find sources
    files = []
    for d in SRC_DIRS:
        # Standard recursive glob
        files.extend(glob.glob(os.path.join(d, "**", "*.c"), recursive=True))

    if not files:
        print("No source files found.")
        sys.exit(0)

    print(f"Linting {len(files)} files (Incremental)...")

    # Parallel execution
    # CPU count + 2 is a common heuristic for I/O + CPU bound tasks
    cpu_count = os.cpu_count() or 1
    max_workers = cpu_count + 2

    failed_files = []

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        results = executor.map(process_file, files)

        linted_count = 0
        skipped_count = 0

        for status, file_path, output in results:
            if status == "SKIPPED":
                skipped_count += 1
                # print(f"  [SKIP] {file_path}") # Too verbose?
            elif status == "LINTED":
                linted_count += 1
                print(f"  [OK]   {file_path}")
                if output.strip():
                    print(output)
            else: # FAILED or ERROR
                failed_files.append(file_path)
                print(f"  [FAIL] {file_path}")
                print(output)

    print(f"\nSummary: {linted_count} linted, {skipped_count} skipped, {len(failed_files)} failed.")

    if failed_files:
        sys.exit(1)

if __name__ == "__main__":
    main()
