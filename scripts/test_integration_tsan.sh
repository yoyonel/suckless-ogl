#!/bin/bash
set -eo pipefail

# Use the TSan-instrumented build
APP_PATH="./build-tsan/app"

# Ensure the app exists
if [ ! -f "$APP_PATH" ]; then
    echo "Error: tsan build not found at $APP_PATH. Run 'just tsan' first."
    exit 1
fi

# Delegate to generic script
SCRIPT_DIR=$(dirname "$0")
env TSAN_OPTIONS="suppressions=tsan_suppressions.txt" "$SCRIPT_DIR/test_integration_generic.sh" "$APP_PATH"
