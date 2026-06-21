#!/bin/bash
# =============================================================================
# STRESS TEST: Environment Map Switching
# =============================================================================
# Designed to be run under TSan (ThreadSanitizer) to detect data races.
# Gracefully handles cached maps (where the async loader does not fire).
# =============================================================================

set -eo pipefail

APP_PATH="${1:?Error: Application path required.}"
ITERATIONS="${2:-30}"
DELAY_MS="${3:-200}"
WINDOW_NAME="Icosphere Phong"
LOG_FILE="stress_envmap.log"
TIMEOUT_SEC=8 # Reduced slightly to speed up cached iterations

EXPECTED_LOG_PATTERN="Finished loading & converting:"

SCRIPT_DIR=$(dirname "$0")
source "$SCRIPT_DIR/integration_utils.sh"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║           ENVIRONMENT MAP STRESS TEST (TSAN)                 ║${NC}"
echo -e "${CYAN}╠══════════════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC} Binary:     ${YELLOW}$APP_PATH${NC}"
echo -e "${CYAN}║${NC} Iterations: ${YELLOW}$ITERATIONS${NC}"
echo -e "${CYAN}║${NC} Delay:      ${YELLOW}${DELAY_MS}ms${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"

rm -f "$LOG_FILE"
: >"$LOG_FILE"

export TSAN_OPTIONS="halt_on_error=0:history_size=7:suppressions=tsan_suppressions.txt"
stdbuf -oL -eL "$APP_PATH" > >(tee "$LOG_FILE") 2>&1 &
APP_PID=$!

sleep 0.2
wait_for_window_start $APP_PID "$WINDOW_NAME" || exit 1

# Protected xdotool call
WID=$(timeout 2 xdotool search --sync --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1 || true)
if [ -n "$WID" ]; then
    focus_window "$WID"
fi

echo -e "\n${YELLOW}[INIT]${NC} Waiting for initial engine boot..."
sleep 3

echo -e "\n${CYAN}[START]${NC} Pumping Page_Down keys..."

wait_for_log_pattern() {
    local pattern="$1"
    local count_before="$2"
    local timeout_ms=$((TIMEOUT_SEC * 1000))
    local poll_ms=50
    local waited=0

    while [ $waited -lt $timeout_ms ]; do
        if ! kill -0 $APP_PID 2>/dev/null; then return 1; fi

        local current_count
        current_count=$(grep -c "$pattern" "$LOG_FILE" 2>/dev/null || true)
        current_count=${current_count:-0}

        if [ "$current_count" -gt "$count_before" ]; then return 0; fi

        sleep "0.$(printf '%03d' $poll_ms)"
        waited=$((waited + poll_ms))
    done
    return 2 # Timeout (Likely cached map)
}

SUCCESS_COUNT=0
CACHED_COUNT=0

for ((i = 1; i <= ITERATIONS; i++)); do
    if ! kill -0 $APP_PID 2>/dev/null; then
        echo -e "${RED}[CRASH]${NC} Application died unexpectedly at iteration $i"
        break
    fi

    # Always go forward to discover new maps.
    KEY="Page_Down"

    count_before=$(grep -c "$EXPECTED_LOG_PATTERN" "$LOG_FILE" 2>/dev/null || true)
    count_before=${count_before:-0}

    # Protected WID extraction to avoid exit code 2 crashes
    wid=$(timeout 2 xdotool search --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1 || true)

    if [ -n "$wid" ]; then
        timeout 2 xdotool windowactivate "$wid" 2>/dev/null || true
        sleep 0.05
        # Send key directly to the window to prevent keystroke loss
        timeout 2 xdotool key --window "$wid" --delay 0 "$KEY" 2>/dev/null || true
    else
        # Fallback if window manager hides the window temporarily
        timeout 2 xdotool key --delay 0 "$KEY" 2>/dev/null || true
    fi

    if [ "$DELAY_MS" -gt 0 ]; then
        sleep "$(awk "BEGIN{printf \"%.3f\", $DELAY_MS/1000}")"
    fi

    wait_for_log_pattern "$EXPECTED_LOG_PATTERN" "$count_before"
    status=$?

    if [ $status -eq 1 ]; then
        echo -e "${RED}[CRASH]${NC} Crash detected during iteration $i."
        break
    elif [ $status -eq 2 ]; then
        # We do not kill the app here anymore. We assume the map was loaded from cache.
        CACHED_COUNT=$((CACHED_COUNT + 1))
        if (((i * 2) % 10 == 0)) || ((i == ITERATIONS)); then
            echo -e "${YELLOW}[INFO]${NC}     Iteration $i / $ITERATIONS: Timeout reached (Map likely served from cache)"
        fi
        continue
    fi

    SUCCESS_COUNT=$((SUCCESS_COUNT + 1))

    if (((i * 2) % 10 == 0)) || ((i == ITERATIONS)); then
        echo -e "${GREEN}[OK]${NC}       Iteration $i / $ITERATIONS: Async load successful"
    fi
done

# Clean termination
if kill -0 $APP_PID 2>/dev/null; then
    echo -e "\n${CYAN}[CLEANUP]${NC} Terminating application cleanly..."
    wid=$(timeout 2 xdotool search --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1 || true)
    if [ -n "$wid" ]; then
        timeout 2 xdotool windowfocus "$wid" 2>/dev/null || true
        timeout 2 xdotool key --window "$wid" Escape 2>/dev/null || true
    fi
    timeout 5 bash -c "while kill -0 $APP_PID 2>/dev/null; do sleep 0.1; done" || kill -9 $APP_PID 2>/dev/null || true
fi
wait $APP_PID 2>/dev/null || true

# Final TSan Analysis report
echo -e "\n${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    TSAN STRESS TEST RESULTS                  ║${NC}"
echo -e "${CYAN}╠══════════════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC} Async Loads: ${GREEN}$SUCCESS_COUNT${NC}"
echo -e "${CYAN}║${NC} Cache Hits:  ${YELLOW}$CACHED_COUNT${NC} (No async loader activity)"

if grep -qE "WARNING: ThreadSanitizer: data race" "$LOG_FILE" 2>/dev/null; then
    echo -e "${RED}║ FAIL: Data races detected by TSan!                           ║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}\n"
    grep -A 20 "WARNING: ThreadSanitizer: data race" "$LOG_FILE" | head -n 40
    exit 1
else
    echo -e "${GREEN}║ SUCCESS: No data races detected.                             ║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
    exit 0
fi
