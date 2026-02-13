#!/bin/bash
set -eo pipefail

# Use the ASan-instrumented build
APP_PATH="./build-asan/app"
WINDOW_NAME="Icosphere Phong"
LOG_FILE="asan_integration.log"

# Ensure the app exists
if [ ! -f "$APP_PATH" ]; then
    echo "Error: ASan build not found at $APP_PATH. Run 'make asan' first."
    exit 1
fi

# Ensure xdotool is installed
if ! command -v xdotool &> /dev/null; then
    echo "Error: xdotool is required for this test."
    exit 1
fi

echo "Starting Application with AddressSanitizer..."
# ASAN_OPTIONS can be used to customize behavior
# exitcode=1 ensures the script fails if ASan detects an issue
export ASAN_OPTIONS="exitcode=1:detect_leaks=1:symbolize=1"
export LSAN_OPTIONS="suppressions=lsan.supp"

# Run the app in background, but capture its PID correctly
# We use a subshell to background the app and the redirection
( $APP_PATH 2>&1 | tee $LOG_FILE ) &
APP_PID=$!

# Wait for App to initialize
# Note: User has disabled these sleeps, which might be risky if the app
# takes time to create the window, but we'll respect the change.
# sleep 2
if ! kill -0 $APP_PID 2>/dev/null; then
    echo "Error: App failed to start or crashed immediately. Check $LOG_FILE"
    exit 1
fi
# sleep $((INIT_WAIT - 2))

echo "Searching for Window: $WINDOW_NAME"
WID=$(xdotool search --sync --name "$WINDOW_NAME" | head -n 1)

if [ -z "$WID" ]; then
    echo "Error: Window '$WINDOW_NAME' not found! Aborting."
    kill $APP_PID || true
    exit 1
fi

echo "Window found: ID $WID"

echo "Attempting to focus window..."
xdotool windowfocus "$WID" || echo "Warning: windowfocus failed (non-fatal)"
xdotool windowactivate "$WID" || echo "Warning: windowactivate failed (expected in headless CI)"

echo "Starting Integration Test Scenario..."
# sleep 1

# 0. Activate Functions

echo "=> FPS Counter (F1)"
for i in {1..6}; do
    xdotool key --delay 200 F1
done
sleep 1

echo "=> Help (F2)"
xdotool key --delay 200 F2
sleep 1
xdotool key --delay 200 F2

echo "=> Activate GPU Profiler (F3)"
xdotool key --delay 200 F3
sleep 1

echo "=> Log on GPU Metrics (F4)"
xdotool key --delay 200 F4
sleep 1

echo "=> Resetting Camera (R)"
xdotool key --delay 200 R
sleep 1

echo "=> Test Full Screen (F)"
xdotool key --delay 200 F
sleep 1
echo "=> Test Windowed (F)"
xdotool key --delay 200 F
sleep 1

echo "=> Test Transition switching mode (T)"
xdotool key --delay 200 T
sleep 1

# 1. Environment Switching
echo "=> Switching Environments (Page_Up / Page_Down)"
xdotool key --delay 200 Page_Up
sleep 1
xdotool key --delay 200 Page_Up
sleep 1
xdotool key --delay 200 Page_Down
sleep 1

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
xdotool key --delay 500 m # Motion Blur
xdotool key --delay 500 x # FXAA

xdotool key --delay 500 w # Wireframe ON
xdotool key --delay 500 w # Wireframe OFF

# 4. Camera Movement
echo "=> Moving Camera"
xdotool keydown z; sleep 0.5; xdotool keyup z
xdotool keydown d; sleep 0.5; xdotool keyup d
xdotool keydown s; sleep 0.5; xdotool keyup s
xdotool keydown a; sleep 0.5; xdotool keyup a
xdotool keydown q; sleep 0.5; xdotool keyup q
xdotool keydown e; sleep 0.5; xdotool keyup e

# 5. PBR Debug Modes
echo "=> Cycling PBR Debug Modes (F5)"
for i in {1..9}; do
    xdotool key --delay 300 F5
done

# 6. Performance Mode
echo "=> Toggling Performance Mode (F9)"
xdotool key --delay 500 F9
sleep 1
xdotool key --delay 500 F9
# sleep 1

# 7. Style 7 - Banding Modes
echo "=> Testing Style 7 - Banding Modes"
for i in {1..4}; do
    xdotool key --delay 500 7
done


echo "=> Test Complete. Exiting..."
xdotool key Escape

if wait $APP_PID; then
    echo "SUCCESS: App exited cleanly and AddressSanitizer found no errors."
    EXIT_CODE=0
else
    echo "FAILURE: AddressSanitizer reported errors or app crashed!"
    EXIT_CODE=1
fi

# Give a moment for logs to flush
# sleep 2

echo "---------------------------------------------------"
echo "AddressSanitizer / LeakSanitizer Report Summary (from $LOG_FILE):"
grep -E "ERROR: (Address|Leak)Sanitizer" $LOG_FILE || echo "No ASan/LSan errors found in log."
echo "---------------------------------------------------"

exit $EXIT_CODE
