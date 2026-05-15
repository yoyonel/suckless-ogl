#!/usr/bin/env bash
# Show the subsystem descriptor init/cleanup call order.
# Parses APP_SUBSYSTEM_TABLE in src/app.c and resolves each descriptor macro
# to its {name, init_fn, cleanup_fn} triple from the include/ headers.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

APP_C="$ROOT_DIR/src/app.c"

# Extract descriptor macro names from APP_SUBSYSTEM_TABLE (skip {0} sentinel)
mapfile -t MACROS < <(
    sed -n '/^static const SubsystemDescriptor APP_SUBSYSTEM_TABLE/,/^};/p' "$APP_C" \
        | grep -oP 'APP_\w+_DESCRIPTOR' \
        | grep -v '^$'
)

if [[ ${#MACROS[@]} -eq 0 ]]; then
    echo "ERROR: No descriptor macros found in $APP_C" >&2
    exit 1
fi

# Resolve each macro to its {name, init, cleanup} triple from headers
declare -a NAMES=()
declare -a INITS=()
declare -a CLEANUPS=()
declare -a SOURCES=()

for macro in "${MACROS[@]}"; do
    # Find the header defining this macro
    header=$(grep -rl "#define $macro" "$ROOT_DIR/include/" 2>/dev/null | head -1)
    if [[ -z "$header" ]]; then
        NAMES+=("???")
        INITS+=("???")
        CLEANUPS+=("???")
        SOURCES+=("???")
        continue
    fi

    # Extract the triple: {"name", init_fn, cleanup_fn}
    # Remove line-continuation backslashes before joining lines
    triple=$(sed -n "/#define $macro/,/}/p" "$header" \
        | sed 's/\\$//' | tr '\n' ' ' | sed 's/.*{//;s/}.*//' | tr -s ' ')

    name=$(echo "$triple" | cut -d',' -f1 | tr -d ' "')
    init_fn=$(echo "$triple" | cut -d',' -f2 | xargs)
    cleanup_fn=$(echo "$triple" | cut -d',' -f3 | xargs)

    # Find the source file containing the init function
    src=$(grep -rl "^int ${init_fn}(" "$ROOT_DIR/src/" 2>/dev/null | head -1 \
        || grep -rl "${init_fn}" "$ROOT_DIR/src/" 2>/dev/null | head -1)
    src="${src#"$ROOT_DIR/"}"

    NAMES+=("$name")
    INITS+=("$init_fn")
    CLEANUPS+=("$cleanup_fn")
    SOURCES+=("${src:-unknown}")
done

COUNT=${#MACROS[@]}

# --- Output ---
echo "Subsystem Descriptor Order (from APP_SUBSYSTEM_TABLE in src/app.c)"
echo "==================================================================="
echo ""
echo "Init order (forward) — app_subsystems_init():"
for ((i = 0; i < COUNT; i++)); do
    printf "  %d: %-12s → %-35s (%s)\n" "$i" "${NAMES[$i]}" "${INITS[$i]}" "${SOURCES[$i]}"
done

echo ""
echo "Cleanup order (reverse) — app_subsystems_cleanup():"
for ((i = COUNT - 1; i >= 0; i--)); do
    printf "  %d: %-12s → %-35s (%s)\n" "$i" "${NAMES[$i]}" "${CLEANUPS[$i]}" "${SOURCES[$i]}"
done

echo ""
echo "Total: $COUNT subsystems"
