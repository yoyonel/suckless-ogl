#!/bin/bash
# Script wrapper pour exécuter les tests avec Xvfb
# Usage: ./run_test_with_xvfb.sh <test_executable>

set -e

TEST_EXEC="$1"

if [ -z "$TEST_EXEC" ]; then
    echo "Usage: $0 <test_executable>"
    exit 1
fi

# Vérifier si Xvfb est installé
if ! command -v Xvfb &> /dev/null; then
    echo "Warning: Xvfb not found. Running test without virtual display."
    exec "$TEST_EXEC"
fi

# Trouver un display libre
DISPLAY_NUM=99
while [ -e "/tmp/.X${DISPLAY_NUM}-lock" ]; do
    DISPLAY_NUM=$((DISPLAY_NUM + 1))
done

# Démarrer Xvfb en arrière-plan
Xvfb :${DISPLAY_NUM} -screen 0 1920x1080x24 > /dev/null 2>&1 &
XVFB_PID=$!

# Attendre que Xvfb démarre
sleep 1

# Exporter DISPLAY
export DISPLAY=:${DISPLAY_NUM}

# Exécuter le test

# Auto-détection du runner si c'est un exécutable Windows (.exe) et que le runner n'est pas défini
if [[ "$TEST_EXEC" == *.exe ]] && [ -z "$TEST_RUNNER_PREFIX" ]; then
    TEST_RUNNER_PREFIX="wine64"
fi

if [ -n "$TEST_RUNNER_PREFIX" ]; then
    # Fallback wine64 -> wine si wine64 n'est pas disponible (cas fréquent en local)
    if [ "$TEST_RUNNER_PREFIX" = "wine64" ] && ! command -v wine64 &> /dev/null && command -v wine &> /dev/null; then
        TEST_RUNNER_PREFIX="wine"
    fi
    $TEST_RUNNER_PREFIX "$TEST_EXEC"
else
    "$TEST_EXEC"
fi
EXIT_CODE=$?

# Nettoyer
if [ -n "$XVFB_PID" ]; then
    kill $XVFB_PID 2>/dev/null || true
fi

exit $EXIT_CODE
