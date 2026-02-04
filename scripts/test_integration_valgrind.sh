#!/bin/bash
set -eo pipefail

# Use the standard Release build for Valgrind (more realistic than ASan for some leaks)
APP_PATH="./build-release/app"
WINDOW_NAME="Icosphere Phong"
LOG_FILE="valgrind_integration.log"

# Ensure the app exists
if [ ! -f "$APP_PATH" ]; then
    echo "Error: Release build not found at $APP_PATH. Run 'make release' first."
    exit 1
fi

# Ensure xdotool is installed
if ! command -v xdotool &> /dev/null; then
    echo "Error: xdotool is required for this test."
    exit 1
fi

echo "Starting Application with Valgrind..."
# --leak-check=full: Detect all leaks
# --error-exitcode=1: Fail script if errors found
# --suppressions: ignore system library noise
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --error-exitcode=1 \
         --log-file=$LOG_FILE.valgrind \
         --suppressions=valgrind.supp \
         $APP_PATH 2>&1 | tee $LOG_FILE &
APP_PID=$!

# Wait for App to initialize
echo "Searching for Window: $WINDOW_NAME"
WID=""
for i in {1..30}; do
    WID=$(xdotool search --name "$WINDOW_NAME" | head -n 1)
    if [ -n "$WID" ]; then break; fi
    sleep 1
done

if [ -z "$WID" ]; then
    echo "Error: Window '$WINDOW_NAME' not found! Aborting."
    kill $APP_PID || true
    exit 1
fi

echo "Window found: ID $WID"
xdotool windowactivate "$WID"

echo "Starting Integration Test Scenario..."
sleep 2

# Test scenario (same as ASan)
xdotool key --delay 200 Page_Up
sleep 1
xdotool key --delay 200 Page_Down
sleep 1
for i in {1..4}; do xdotool key --delay 500 $i; done
xdotool key --delay 500 v
xdotool key --delay 500 g
xdotool key --delay 500 b

echo "=> Test Complete. Exiting..."
xdotool key Escape

if wait $APP_PID; then
    echo "SUCCESS: App exited cleanly and Valgrind found no errors."
    EXIT_CODE=0
else
    echo "FAILURE: Valgrind reported errors or app crashed!"
    EXIT_CODE=1
fi

echo "---------------------------------------------------"
tail -n 20 $LOG_FILE.valgrind || true
echo "---------------------------------------------------"

exit $EXIT_CODE
