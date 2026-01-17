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
"$TEST_EXEC"
EXIT_CODE=$?

# Nettoyer
kill $XVFB_PID 2>/dev/null || true

exit $EXIT_CODE