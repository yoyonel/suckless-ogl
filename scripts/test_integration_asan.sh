#!/bin/bash
set -eo pipefail

# Use the ASan-instrumented build
APP_PATH="./build-asan/app"

# Ensure the app exists
if [ ! -f "$APP_PATH" ]; then
    echo "Error: ASan build not found at $APP_PATH. Run 'just asan' first."
    exit 1
fi

# Delegate to generic script
SCRIPT_DIR=$(dirname "$0")
"$SCRIPT_DIR/test_integration_generic.sh" "$APP_PATH"
