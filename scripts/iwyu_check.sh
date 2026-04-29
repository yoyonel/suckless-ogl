#!/usr/bin/env bash
# iwyu_check.sh — Include-What-You-Use checker for pre-push and CI.
#
# Modes:
#   --staged    Check staged .c files only (for git hooks)
#   --changed   Check files changed vs BASE_REF (default: origin/master)
#   --full      Check all src/*.c files (CI-grade)
#   <files...>  Check explicit file list
#
# Environment:
#   IWYU_BUILD_DIR  Override build directory (default: build)
#   BASE_REF        Override base ref for --changed mode (default: origin/master)
#
# Exit codes:
#   0  All clean (or skipped: no files, no IWYU, no compile_commands.json)
#   1  Unused includes detected

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${IWYU_BUILD_DIR:-build}"
MAPPING_FILE="${ROOT_DIR}/.iwyu.imp"

# --- Known false positives (IWYU suggests removing, but they are needed) ---
# Each pattern is a grep -E pattern matched against "- #include ..." lines.
# Update this list when false positives are confirmed (see docs/iwyu_audit.md).
ALLOWLIST=(
    'gl_common\.h'      # Provides RAII macros (GL_CHECK, GL_GROUP), not just glad.h
    'cglm/cglm\.h'      # Umbrella header — IWYU wants sub-headers but cglm docs say use this
    'postprocess\.h'     # app.h uses PostProcessState from postprocess.h (transitive struct)
    'sched\.h'           # struct sched_param in struct field (perf_mode.h)
    'immintrin\.h'       # AVX/F16C umbrella header (simd_utils.c)
    'GLFW/glfw3\.h'      # app_binding.h API is about GLFW key bindings (design choice)
    'gpu_profiler\.h'    # effect_benchmark.h uses GPUProfiler* in struct (IWYU misses it)
    '"app\.h"'           # app_ui.c uses App transitively — defensive include
)

# --- Argument parsing ---
MODE=""
FILES=()
VERBOSE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --staged|--changed|--full)
            MODE="$1"; shift ;;
        --verbose|-v)
            VERBOSE=1; shift ;;
        --build-dir|-p)
            BUILD_DIR="$2"; shift 2 ;;
        -*)
            echo "Unknown option: $1" >&2; exit 1 ;;
        *.c)
            FILES+=("$1"); shift ;;
        *)
            # Treat bare directory/name as build dir
            BUILD_DIR="$1"; shift ;;
    esac
done

# --- Preflight ---
if ! command -v include-what-you-use &>/dev/null; then
    echo "⚠ IWYU not installed, skipping check"
    exit 0
fi

if ! command -v iwyu_tool &>/dev/null; then
    echo "⚠ iwyu_tool not found, skipping check"
    exit 0
fi

COMPILE_DB="${ROOT_DIR}/${BUILD_DIR}/compile_commands.json"
if [[ ! -f "$COMPILE_DB" ]]; then
    echo "⚠ No compile_commands.json in ${BUILD_DIR}/, skipping IWYU check"
    echo "  Run: just build (or cmake -B ${BUILD_DIR} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)"
    exit 0
fi

# --- Determine files to check ---
cd "$ROOT_DIR"

case "$MODE" in
    --staged)
        mapfile -t FILES < <(git diff --cached --name-only --diff-filter=ACM -- '*.c' | grep -v '^tests/' || true)
        ;;
    --changed)
        BASE="${BASE_REF:-origin/master}"
        if ! git rev-parse --verify "$BASE" &>/dev/null; then
            echo "⚠ Base ref '${BASE}' not available, skipping IWYU check"
            exit 0
        fi
        mapfile -t FILES < <(git diff --name-only "$BASE" -- '*.c' | grep -v '^tests/' || true)
        ;;
    --full)
        mapfile -t FILES < <(find src -name '*.c' | sort)
        ;;
    "")
        if [[ ${#FILES[@]} -eq 0 ]]; then
            echo "Usage: $0 [--staged|--changed|--full] [--build-dir DIR] [files...]"
            exit 1
        fi
        ;;
esac

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "✓ IWYU: no C source files to check"
    exit 0
fi

echo "IWYU: checking ${#FILES[@]} file(s)..."

# --- Build IWYU arguments ---
IWYU_EXTRA_ARGS=("-Xiwyu" "--no_fwd_decls")
if [[ -f "$MAPPING_FILE" ]]; then
    IWYU_EXTRA_ARGS+=("-Xiwyu" "--mapping_file=${MAPPING_FILE}")
fi

# --- Build allowlist grep pattern ---
EXCLUDE_PATTERN=""
for pat in "${ALLOWLIST[@]}"; do
    EXCLUDE_PATTERN="${EXCLUDE_PATTERN:+${EXCLUDE_PATTERN}|}${pat}"
done

# --- Run IWYU and analyze ---
ISSUES=0
ISSUE_FILES=()
TMPFILE=$(mktemp)
trap 'rm -f "$TMPFILE"' EXIT

for file in "${FILES[@]}"; do
    [[ -f "$file" ]] || continue

    # Run iwyu_tool (may process multiple compile_commands entries)
    iwyu_tool -p "$BUILD_DIR" "$file" -- "${IWYU_EXTRA_ARGS[@]}" > "$TMPFILE" 2>&1 || true

    # Extract "should remove" lines, deduplicate (multiple compile_commands entries)
    REMOVALS=$(awk '/should remove these lines/,/^$/' "$TMPFILE" \
        | grep '^- #include' \
        | sort -u || true)

    # Filter known false positives
    if [[ -n "$EXCLUDE_PATTERN" && -n "$REMOVALS" ]]; then
        REMOVALS=$(echo "$REMOVALS" | grep -Ev "$EXCLUDE_PATTERN" || true)
    fi

    if [[ -n "$REMOVALS" ]]; then
        echo ""
        echo "❌ ${file}:"
        while IFS= read -r line; do
            echo "    ${line}"
        done <<< "$REMOVALS"
        ISSUES=$((ISSUES + 1))
        ISSUE_FILES+=("$file")
    elif [[ $VERBOSE -eq 1 ]]; then
        echo "  ✓ ${file}"
    fi
done

echo ""
if [[ $ISSUES -gt 0 ]]; then
    echo "❌ IWYU: unused includes in ${ISSUES} file(s): ${ISSUE_FILES[*]}"
    echo "  Fix: remove the flagged #include lines"
    echo "  False positive? Add pattern to ALLOWLIST in scripts/iwyu_check.sh"
    echo "  Docs: docs/iwyu_audit.md"
    exit 1
else
    echo "✓ IWYU: all ${#FILES[@]} file(s) clean"
fi
