#!/bin/bash

check_dependencies() {
    if ! command -v xdotool &> /dev/null; then
        echo "Error: xdotool is required for this test."
        exit 1
    fi
}

wait_for_window_start() {
    local pid=$1
    local window_name=$2

    # Wait for App to initialize
    # We can rely on a loop checking for the window instead of a hard sleep if preferred,
    # or just a short sleep to let the process start.
    # The original scripts had different sleep strategies, we'll standardize on a loop.

    echo "Waiting for '$window_name' (PID $pid)..."

    local max_retries=30
    local found_wid=""

    for ((i=0; i<max_retries; i++)); do
        if ! kill -0 $pid 2>/dev/null; then
            echo "Error: App process $pid died unexpectedly."
            return 1
        fi

        found_wid=$(xdotool search --sync --onlyvisible --name "$window_name" 2>/dev/null | head -n 1)
        if [ -n "$found_wid" ]; then
            echo "Window found: ID $found_wid"
            echo "$found_wid" # Return WID via stdout pattern if needed, or set global
            return 0
        fi
        sleep 0.5
    done

    echo "Error: Window '$window_name' not found after waiting."
    return 1
}

focus_window() {
    local wid=$1
    echo "Targeting window $wid for events..."
    # We use windowfocus but NOT windowactivate to avoid bringing it to the front
    # on some window managers, though results may vary.
    xdotool windowfocus "$wid" || echo "Warning: windowfocus failed"
}

send_key() {
    local wid=$1
    local key=$2
    local delay=${3:-200}
    # --window $wid attempts to send the event directly to the window ID
    xdotool key --window "$wid" --delay "$delay" "$key"
}

send_keydown() {
    local wid=$1
    local key=$2
    xdotool keydown --window "$wid" "$key"
}

send_keyup() {
    local wid=$1
    local key=$2
    xdotool keyup --window "$wid" "$key"
}
