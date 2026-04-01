#!/bin/bash
set -eo pipefail

# Usage: ./test_integration_generic.sh [RUNNER] <path_to_app>
RUNNER=""
if [ "$1" == "wine" ]; then
    RUNNER="wine"
    shift
fi

if [ -z "$1" ]; then
    echo "Error: No application path provided."
    echo "Usage: $0 [RUNNER] <path_to_app>"
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
( $RUNNER $APP_PATH 2>&1 | tee $LOG_FILE ) &
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
    echo "SUCCESS: App exited cleanly."
    EXIT_CODE=0
else
    echo "FAILURE: App crashed or exited with error!"
    EXIT_CODE=1
fi

# Optional: Report summary (Conditional on ASan presence)
if nm -u "$APP_PATH" 2>/dev/null | grep -q "__asan_init"; then
    echo "---------------------------------------------------"
    echo "AddressSanitizer / LeakSanitizer Report Summary (from $LOG_FILE):"
    grep -E "ERROR: (Address|Leak)Sanitizer" $LOG_FILE || echo "No ASan/LSan errors found in log."
    echo "---------------------------------------------------"
else
    # For non-ASan builds, just show the last few lines of the log to confirm clean exit
    echo "---------------------------------------------------"
    echo "Integration Test Log Summary (tail of $LOG_FILE):"
    tail -n 5 "$LOG_FILE"
    echo "---------------------------------------------------"
fi

exit $EXIT_CODE
