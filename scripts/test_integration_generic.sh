#!/bin/bash
set -eo pipefail

# Usage: ./test_integration_generic.sh <path_to_app>
if [ -z "$1" ]; then
    echo "Error: No application path provided."
    echo "Usage: $0 <path_to_app>"
    exit 1
fi

APP_PATH="$1"
WINDOW_NAME="Icosphere Phong"
LOG_FILE="integration_test.log"

# Source shared utilities and scenarios
SCRIPT_DIR=$(dirname "$0")
source "$SCRIPT_DIR/integration_utils.sh"
source "$SCRIPT_DIR/integration_scenarios.sh"

check_dependencies

if [ ! -f "$APP_PATH" ]; then
    echo "Error: Application binary not found at $APP_PATH."
    exit 1
fi

echo "Starting Application..."
# ASAN_OPTIONS/LSAN_OPTIONS are harmless on non-ASan builds, but useful if ASan is active.
export ASAN_OPTIONS="exitcode=1:detect_leaks=1:symbolize=1"
export LSAN_OPTIONS="suppressions=lsan.supp"

# Run the app in background, but capture its PID correctly
# We use a subshell to background the app and the redirection
( $APP_PATH 2>&1 | tee $LOG_FILE ) &
APP_PID=$!

if ! wait_for_window_start $APP_PID "$WINDOW_NAME"; then
    echo "Aborting test due to window not found."
    kill $APP_PID || true
    exit 1
fi

# Find WID again or pass it from function (we'll just find it again for simplicity)
WID=$(xdotool search --sync --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1)
focus_window "$WID"

echo "Starting Integration Test Scenario..."
run_scenario_full

if wait $APP_PID; then
    echo "SUCCESS: App exited cleanly and AddressSanitizer found no errors."
    EXIT_CODE=0
else
    echo "FAILURE: AddressSanitizer reported errors or app crashed!"
    EXIT_CODE=1
fi

echo "---------------------------------------------------"
echo "AddressSanitizer / LeakSanitizer Report Summary (from $LOG_FILE):"
grep -E "ERROR: (Address|Leak)Sanitizer" $LOG_FILE || echo "No ASan/LSan errors found in log."
echo "---------------------------------------------------"

exit $EXIT_CODE
