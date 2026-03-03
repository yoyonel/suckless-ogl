#!/bin/bash
set -e

# verify_apitrace_perf.sh
# Automates the detection of OpenGL performance issues using ApiTrace.

APITRACE_BIN=${1:-"apitrace"}
TEST_BIN="./build/tests/test_app"
TRACE_FILE="build/test_integration.trace"

# 1. Resolve ApiTrace wrappers path
APITRACE_DIR=$(dirname $(dirname $(readlink -f $(which $APITRACE_BIN 2>/dev/null || echo $APITRACE_BIN))))
WRAPPERS_PATH=""

# Common locations for wrappers (relative to APITRACE_DIR)
for loc in \
    "lib/apitrace/wrappers" \
    "lib64/apitrace/wrappers" \
    "lib/x86_64-linux-gnu/apitrace/wrappers" \
    "lib/i386-linux-gnu/apitrace/wrappers"; do
    if [ -d "$APITRACE_DIR/$loc" ]; then
        WRAPPERS_PATH="$APITRACE_DIR/$loc"
        break
    fi
done

if [ -z "$WRAPPERS_PATH" ]; then
    echo "❌ Error: Could not find ApiTrace wrappers in $APITRACE_DIR"
    exit 1
fi

echo "[*] Using ApiTrace from: $APITRACE_DIR"
echo "[*] Wrappers path: $WRAPPERS_PATH"

# 2. Record trace
echo "[*] Recording trace of $TEST_BIN..."
rm -f "$TRACE_FILE"

# Run with xvfb-run to avoid GUI popping up if not needed
XVFB=""
if command -v xvfb-run >/dev/null 2>&1; then
    XVFB="xvfb-run -a"
fi

# We use apitrace trace to handle LD_PRELOAD and output path correctly
if [ -n "$XVFB" ]; then
    $XVFB "$APITRACE_BIN" trace --api gl --output "$TRACE_FILE" "$TEST_BIN"
else
    "$APITRACE_BIN" trace --api gl --output "$TRACE_FILE" "$TEST_BIN"
fi

if [ ! -f "$TRACE_FILE" ]; then
    # ApiTrace might name it app.trace if not specified
    if [ -f "app.trace" ]; then
        mv "app.trace" "$TRACE_FILE"
    else
        echo "❌ Error: Trace file was not generated."
        exit 1
    fi
fi

echo "✅ Trace recorded: $TRACE_FILE"

# 3. Analyze trace for performance issues
echo "[*] Analyzing trace for performance stalls..."

# We grep for "performance issue" or "stall" in the dump
# Note: we use --color=never to avoid escape codes in output
STALLS=$("$APITRACE_BIN" dump --color=never "$TRACE_FILE" | grep -Ei "performance issue|stall" | grep -v "Message" || true)

if [ -n "$STALLS" ]; then
    echo "⚠️  PERFORMANCE REGRESSIONS DETECTED:"
    echo "--------------------------------------------------"
    echo "$STALLS" | sort -u
    echo "--------------------------------------------------"
    echo "❌ Validation failed: Performance issues found in trace."
    exit 1
else
    echo "✅ Success: No OpenGL performance issues detected."
    exit 0
fi
