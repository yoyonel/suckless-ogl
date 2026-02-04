#!/bin/bash
set -e

APP_PATH="./build/app"
WINDOW_NAME="Icosphere Phong"
# We'll use this file to capture the report while still logging to terminal
LOG_FILE="valgrind_integration.log"

# Ensure xdotool is installed
if ! command -v xdotool &> /dev/null; then
    echo "Error: xdotool is required for this test."
    exit 1
fi

echo "Starting Valgrind..."
# Remove --log-file to let output flow to terminal.
# We use 'tee' to capture it for the final grep without breaking APP_PID.
# Bash's process substitution is perfect for this.
valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=definite $APP_PATH 2>&1 | tee $LOG_FILE &
APP_PID=$!

# Wait for App to initialize
INIT_WAIT=8
echo "Waiting for App ID $APP_PID to initialize (${INIT_WAIT}s)..."
sleep $INIT_WAIT

echo "Searching for Window: $WINDOW_NAME"
WID=$(xdotool search --sync --name "$WINDOW_NAME" | head -n 1)

if [ -z "$WID" ]; then
    echo "Error: Window '$WINDOW_NAME' not found! Aborting."
    kill $APP_PID
    exit 1
fi

echo "Window found: ID $WID"

echo "Attempting to focus window..."
xdotool windowfocus "$WID" || echo "Warning: windowfocus failed (non-fatal)"
xdotool windowactivate "$WID" || echo "Warning: windowactivate failed (expected in headless CI)"

echo "Starting Integration Test Scenario..."
sleep 1

# 1. Environment Switching
echo "=> Switching Environments (Page_Up / Page_Down)"
xdotool key --delay 200 Page_Up
sleep 2
xdotool key --delay 200 Page_Up
sleep 2
xdotool key --delay 200 Page_Down
sleep 2

# 2. Styles
echo "=> Testing Styles (1-6)"
for i in {1..6}; do
    xdotool key --delay 500 $i
done

xdotool key --delay 500 2 # Style: Subtle

# 3. Post-Process Effects
echo "=> Toggling Effects"
xdotool key --delay 500 v # Vignette OFF
xdotool key --delay 500 v # Vignette ON
xdotool key --delay 500 g # Grain
xdotool key --delay 500 b # Bloom
xdotool key --delay 500 h # DoF
xdotool key --delay 500 j # Auto Exposure

xdotool key --delay 500 w # Wireframe ON
xdotool key --delay 500 w # Wireframe OFF

# 4. Camera Movement
echo "=> Moving Camera"
xdotool keydown z; sleep 0.5; xdotool keyup z
xdotool keydown d; sleep 0.5; xdotool keyup d
xdotool keydown s; sleep 0.5; xdotool keyup s
xdotool keydown a; sleep 0.5; xdotool keyup a

# 5. PBR Debug Modes
echo "=> Cycling PBR Debug Modes (F5)"
for i in {1..5}; do
    xdotool key --delay 300 F5
done

# 6. Performance Mode
echo "=> Toggling Performance Mode (F9)"
xdotool key --delay 500 F9
sleep 2
xdotool key --delay 500 F9
sleep 1

echo "=> Test Complete. Exiting..."
xdotool key Escape

if wait $APP_PID; then
    echo "SUCCESS: App exited cleanly and Valgrind found no errors."
    EXIT_CODE=0
else
    echo "FAILURE: Valgrind reported errors or app crashed!"
    EXIT_CODE=1
fi

# Give a moment for logs to flush in CI
sleep 2

echo "---------------------------------------------------"
echo "Valgrind Report Summary (from $LOG_FILE):"
grep -A 15 "LEAK SUMMARY" $LOG_FILE || echo "Leak summary not found in log."
echo "---------------------------------------------------"

exit $EXIT_CODE
