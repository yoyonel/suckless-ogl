#!/bin/bash
set -e

APP_PATH="./build/app"
WINDOW_NAME="Icosphere Phong"
LOG_FILE="valgrind_integration.log"

# Source shared utilities and scenarios
SCRIPT_DIR=$(dirname "$0")
source "$SCRIPT_DIR/integration_utils.sh"
source "$SCRIPT_DIR/integration_scenarios.sh"

check_dependencies

if [ ! -f "$APP_PATH" ]; then
    echo "Error: Application binary not found at $APP_PATH."
    exit 1
fi

echo "Starting Valgrind..."
# Remove --log-file to let output flow to terminal.
# We use 'tee' to capture it for the final grep without breaking APP_PID.
# Bash's process substitution is perfect for this.
valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=definite "$APP_PATH" 2>&1 | tee "$LOG_FILE" &
APP_PID=$!

if ! wait_for_window_start $APP_PID "$WINDOW_NAME"; then
    echo "Aborting test due to window not found."
    kill $APP_PID
    exit 1
fi

# Find WID again
WID=$(xdotool search --sync --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1)
focus_window "$WID"

echo "Starting Integration Test Scenario..."
run_scenario_minimal

if wait $APP_PID; then
    echo "SUCCESS: App exited cleanly and Valgrind found no errors."
    EXIT_CODE=0
else
    echo "FAILURE: Valgrind reported errors or app crashed!"
    EXIT_CODE=1
fi

echo "---------------------------------------------------"
echo "Valgrind Report Summary (from $LOG_FILE):"
grep -A 15 "LEAK SUMMARY" $LOG_FILE || echo "Leak summary not found in log."
echo "---------------------------------------------------"

exit $EXIT_CODE
