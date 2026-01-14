#!/bin/bash
set -e

# Start Xvfb in background
Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
XVFB_PID=$!

# Wait for Xvfb to start
sleep 2

# Set DISPLAY
export DISPLAY=:99

# Run the application
./app

# Capture exit code
EXIT_CODE=$?

# Cleanup
kill $XVFB_PID 2>/dev/null || true

exit $EXIT_CODE