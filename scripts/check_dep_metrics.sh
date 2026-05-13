#!/usr/bin/env bash
# Dependency metrics gate — fails CI if thresholds exceeded
# Related: https://github.com/yoyonel/suckless-ogl/issues/266
set -euo pipefail

EDGE_LIMIT=${1:-567}
HEADER_FAN_LIMIT=${2:-18}
SOURCE_FAN_LIMIT=${3:-22}

# Count total include edges (only local includes)
total=0
for f in src/*.c src/effects/*.c include/*.h include/effects/*.h; do
    [[ -f "$f" ]] || continue
    n=$(grep -c '#include "' "$f" 2>/dev/null || true)
    total=$((total + n))
done

# Find max header fan-out (all includes)
max_header=0
max_header_file=""
for f in include/*.h include/effects/*.h; do
    [[ -f "$f" ]] || continue
    n=$(grep -c '#include' "$f" 2>/dev/null || true)
    if [[ $n -gt $max_header ]]; then
        max_header=$n
        max_header_file="$f"
    fi
done

# Find max source fan-out (all includes)
max_source=0
max_source_file=""
for f in src/*.c src/effects/*.c; do
    [[ -f "$f" ]] || continue
    n=$(grep -c '#include' "$f" 2>/dev/null || true)
    if [[ $n -gt $max_source ]]; then
        max_source=$n
        max_source_file="$f"
    fi
done

echo "=== Dependency Metrics ==="
echo "Total include edges: $total (limit: $EDGE_LIMIT)"
echo "Max header fan-out:  $max_header [$max_header_file] (limit: $HEADER_FAN_LIMIT)"
echo "Max source fan-out:  $max_source [$max_source_file] (limit: $SOURCE_FAN_LIMIT)"
echo ""

fail=0
if [[ $total -gt $EDGE_LIMIT ]]; then
    echo "FAIL: total include edges ($total) exceeds limit ($EDGE_LIMIT)"
    fail=1
fi
if [[ $max_header -gt $HEADER_FAN_LIMIT ]]; then
    echo "FAIL: header fan-out ($max_header in $max_header_file) exceeds limit ($HEADER_FAN_LIMIT)"
    fail=1
fi
if [[ $max_source -gt $SOURCE_FAN_LIMIT ]]; then
    echo "FAIL: source fan-out ($max_source in $max_source_file) exceeds limit ($SOURCE_FAN_LIMIT)"
    fail=1
fi

if [[ $fail -eq 0 ]]; then
    echo "OK: all dependency metrics within thresholds"
fi
exit $fail
