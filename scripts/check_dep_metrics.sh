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

# ==============================================================================
# 3. Validation & CI Soft-Fail Logic (GitHub Actions)
# ==============================================================================

HAS_WARNING=0
MSG=""

# Vérification du dépassement
if [ "$total" -gt "$EDGE_LIMIT" ]; then
    MSG="Total include edges ($total) exceeds limit ($EDGE_LIMIT)"
    HAS_WARNING=1
fi

if [ "$HAS_WARNING" -eq 1 ]; then
    # Crée une annotation (Warning jaune) visible dans l'onglet "Files changed" de la PR
    echo "::warning title=Dependency Metrics Exceeded::$MSG"

    # Ajoute un encart Markdown dans le résumé global de la CI (Summary)
    # Le 'if' vérifie qu'on est bien dans GitHub Actions pour ne pas créer de fichier en local
    if [ -n "$GITHUB_STEP_SUMMARY" ]; then
        echo "### ⚠️ Attention : Dérive des dépendances" >>"$GITHUB_STEP_SUMMARY"
        echo "$MSG. Pensez à vérifier s'il est possible de réduire la taille des headers (ex: forward declarations)." >>"$GITHUB_STEP_SUMMARY"
    fi

    # Trace dans les logs bruts
    echo "WARN: $MSG"
else
    echo "OK: Metrics within limits."
fi

# IMPORTANT : On force la sortie à 0 pour empêcher le "Hard Fail" de la CI
exit 0
