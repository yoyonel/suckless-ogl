#!/bin/bash
set -e

# scripts/test_integration_apitrace.sh
# Records and analyzes an ApiTrace of the full application running a test scenario.

APP_PATH="./build/app"
WINDOW_NAME="Icosphere Phong"
TRACE_FILE="build/integration.trace"
APITRACE_BIN=${1:-"apitrace"}

# Source shared utilities and scenarios
SCRIPT_DIR=$(dirname "$0")
source "$SCRIPT_DIR/integration_utils.sh"
source "$SCRIPT_DIR/integration_scenarios.sh"

check_dependencies() {
    if ! command -v "$APITRACE_BIN" >/dev/null 2>&1; then
        echo "❌ Error: apitrace binary not found or not executable: $APITRACE_BIN"
        exit 1
    fi
}

check_dependencies

if [ ! -f "$APP_PATH" ]; then
    echo "❌ Error: Application binary not found at $APP_PATH."
    exit 1
fi

echo "[*] Recording trace of $APP_PATH..."
rm -f "$TRACE_FILE"

# 1. Start App with ApiTrace trace
# We use --api gl for OpenGL
"$APITRACE_BIN" trace --api gl --output "$TRACE_FILE" "$APP_PATH" &
APP_PID=$!

# 2. Wait for window to appear
if ! wait_for_window_start $APP_PID "$WINDOW_NAME"; then
    echo "❌ Aborting: Window not found."
    kill $APP_PID 2>/dev/null || true
    exit 1
fi

# 3. Find WID and focus
WID=$(xdotool search --sync --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1)
focus_window "$WID"

# 4. Run GUI Scenario
echo "[*] Running Integration Test Scenario (Full)..."
run_scenario_full

# 5. Wait for app to finish (usually Escape in scenario ends it)
if wait $APP_PID; then
    echo "✅ Trace recorded: $TRACE_FILE"
else
    # Sometimes wait fails if process was already gone, but we check if trace exists
    if [ ! -f "$TRACE_FILE" ]; then
        echo "❌ Error: App crashed or trace not generated."
        exit 1
    fi
    echo "✅ App exited."
fi

# 6. Analyze Trace
echo "[*] Analyzing trace for performance stalls..."
STALLS=$("$APITRACE_BIN" dump --color=never "$TRACE_FILE" | grep -Ei "performance issue|stall" | grep -v "Message" || true)

if [ -n "$STALLS" ]; then
    echo "⚠️  PERFORMANCE REGRESSIONS DETECTED:"
    echo "--------------------------------------------------"
    echo "$STALLS" | sort -u
    echo "--------------------------------------------------"
    echo "❌ Validation failed: Performance issues found in integration trace."
    exit 1
else
    echo "✅ Success: No OpenGL performance issues detected in integration scenario."
    exit 0
fi
