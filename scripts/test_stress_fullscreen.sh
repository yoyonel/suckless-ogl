#!/bin/bash
# =============================================================================
# STRESS TEST: Window ↔ Fullscreen Toggle
# =============================================================================
# Detects deadlocks by monitoring application LOG OUTPUT rather than window
# visibility (a frozen/deadlocked app keeps its window visible + process alive).
#
# Detection strategy:
#   After each 'F' keystroke, wait for the app to log either:
#     "Switched to fullscreen"  or  "Switched to windowed"
#   If the expected log line does NOT appear within TIMEOUT_SEC → DEADLOCK.
#
# Usage:
#   ./scripts/test_stress_fullscreen.sh <path_to_app> [iterations] [delay_ms]
#
# Examples:
#   ./scripts/test_stress_fullscreen.sh ./build/app           # 100 cycles, 50ms
#   ./scripts/test_stress_fullscreen.sh ./build/app 200 10    # aggressive
#   ./scripts/test_stress_fullscreen.sh ./build/app 50 200    # gentle
# =============================================================================

set -eo pipefail

# --- Configuration ---
APP_PATH="${1:?Error: No application path provided. Usage: $0 <app_path> [iterations] [delay_ms]}"
ITERATIONS="${2:-100}"
DELAY_MS="${3:-50}"
WINDOW_NAME="Icosphere Phong"
LOG_FILE="stress_fullscreen.log"
STACKS_FILE="stress_fullscreen.stacks"
TIMEOUT_SEC=5  # Max seconds to wait for toggle acknowledgment in logs

# --- Source shared utilities ---
SCRIPT_DIR=$(dirname "$0")
source "$SCRIPT_DIR/integration_utils.sh"
check_dependencies

if [ ! -f "$APP_PATH" ]; then
    echo "Error: Application binary not found at $APP_PATH."
    exit 1
fi

# --- Color output helpers ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║           FULLSCREEN TOGGLE STRESS TEST                     ║${NC}"
echo -e "${CYAN}╠══════════════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC} Binary:     ${YELLOW}$APP_PATH${NC}"
echo -e "${CYAN}║${NC} Iterations: ${YELLOW}$ITERATIONS${NC}"
echo -e "${CYAN}║${NC} Delay:      ${YELLOW}${DELAY_MS}ms${NC}"
echo -e "${CYAN}║${NC} Timeout:    ${YELLOW}${TIMEOUT_SEC}s per toggle${NC}"
echo -e "${CYAN}║${NC} Detection:  ${YELLOW}Log-based (watches for Switched to ... messages)${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"

# --- Clean previous artifacts ---
rm -f "$LOG_FILE" "$STACKS_FILE"
: > "$LOG_FILE"  # Create empty log file now (needed for tail -f)

# --- Launch application ---
echo -e "\n${CYAN}[INIT]${NC} Starting application..."
export ASAN_OPTIONS="exitcode=1:detect_leaks=1:symbolize=1:halt_on_error=0:verify_asan_link_order=0"
export LSAN_OPTIONS="suppressions=lsan.supp"

# Run app, tee stdout+stderr to log. Use stdbuf to line-buffer so we see
# log lines as soon as they're printed (critical for our detection strategy).
stdbuf -oL -eL $APP_PATH > >(tee "$LOG_FILE") 2>&1 &
APP_PID=$!

# Give stdbuf/tee time to set up
sleep 0.2

if ! wait_for_window_start $APP_PID "$WINDOW_NAME"; then
    echo -e "${RED}[FATAL]${NC} Window not found. Aborting."
    kill $APP_PID 2>/dev/null || true
    exit 1
fi

WID=$(xdotool search --sync --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1)
focus_window "$WID"

# Give the app a few frames to fully initialize (IBL load, shaders, etc.)
sleep 2

echo -e "\n${CYAN}[START]${NC} Beginning stress test: $ITERATIONS fullscreen toggle cycles"
echo ""

# --- Tracking ---
SUCCESS_COUNT=0
HANG_COUNT=0
CRASH_DETECTED=false
START_TIME=$(date +%s)

# --- Core detection function ---
# Waits for a specific pattern to appear in the log file.
# Returns 0 on success, 1 on crash, 2 on timeout (deadlock).
wait_for_log_pattern() {
    local pattern="$1"
    local line_before="$2"
    local timeout_ms=$((TIMEOUT_SEC * 1000))
    local poll_ms=50
    local waited=0

    while [ $waited -lt $timeout_ms ]; do
        # Check process is still alive
        if ! kill -0 $APP_PID 2>/dev/null; then
            return 1  # Crash
        fi

        # Count lines matching pattern AFTER the line we snapshotted
        local current_count
        current_count=$(grep -c "$pattern" "$LOG_FILE" 2>/dev/null || true)
        current_count=${current_count:-0}
        if [ "$current_count" -gt "$line_before" ]; then
            return 0  # Pattern found — toggle succeeded
        fi

        sleep 0.$(printf '%03d' $poll_ms)
        waited=$((waited + poll_ms))
    done

    # Timeout: the app did not produce the expected log line
    # This means the app is hung — DEADLOCK
    return 2
}

# --- Stack trace capture ---
capture_stacks() {
    local iteration=$1
    local direction=$2

    echo -e "${YELLOW}[DIAG]${NC} Capturing thread stack traces (iteration $iteration, $direction)..."

    {
        echo "============================================================"
        echo "DEADLOCK at iteration $iteration ($direction)"
        echo "Timestamp: $(date -Iseconds)"
        echo "PID: $APP_PID"
        echo "============================================================"
    } >> "$STACKS_FILE"

    if command -v gdb &>/dev/null; then
        gdb -batch \
            -ex "set pagination off" \
            -ex "thread apply all bt full" \
            -ex "info threads" \
            -ex "detach" \
            -p $APP_PID 2>/dev/null >> "$STACKS_FILE" || true
        echo -e "${YELLOW}[DIAG]${NC} GDB stack traces saved to ${BOLD}$STACKS_FILE${NC}"
    else
        echo -e "${YELLOW}[DIAG]${NC} GDB not available. Dumping /proc/$APP_PID/status instead."
        {
            echo "--- /proc/$APP_PID/status ---"
            cat /proc/$APP_PID/status 2>/dev/null || echo "(not available)"
            echo ""
            echo "--- /proc/$APP_PID/wchan ---"
            cat /proc/$APP_PID/wchan 2>/dev/null || echo "(not available)"
            echo ""
            # Show all thread stacks via /proc
            for tid_dir in /proc/$APP_PID/task/*/; do
                tid=$(basename "$tid_dir")
                echo "--- Thread $tid stack ---"
                cat /proc/$APP_PID/task/$tid/stack 2>/dev/null || echo "(not available)"
                echo ""
            done
        } >> "$STACKS_FILE"
        echo -e "${YELLOW}[DIAG]${NC} /proc stack info saved to ${BOLD}$STACKS_FILE${NC}"
    fi
}

# --- Determine expected initial state ---
# The app starts in windowed mode, so the first 'F' should go fullscreen.
# We track expected next state to know which log line to wait for.
EXPECT_FULLSCREEN=true

# --- Helper: send keystroke with timeout protection ---
# xdotool can hang if the window is frozen (especially --sync variants).
# We wrap EVERY xdotool interaction in a hard timeout.
XDOTOOL_TIMEOUT=2  # seconds

send_toggle_key() {
    local wid

    # Find the window (may have a new WID after fullscreen toggles)
    wid=$(timeout "$XDOTOOL_TIMEOUT" xdotool search --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1)
    if [ -z "$wid" ]; then
        # Window temporarily invisible during mode transition — try without --onlyvisible
        sleep 0.3
        wid=$(timeout "$XDOTOOL_TIMEOUT" xdotool search --name "$WINDOW_NAME" 2>/dev/null | head -n 1)
    fi

    if [ -n "$wid" ]; then
        # NO --sync flags! They block forever on a frozen window.
        timeout "$XDOTOOL_TIMEOUT" xdotool windowfocus "$wid" 2>/dev/null || true
        timeout "$XDOTOOL_TIMEOUT" xdotool windowactivate "$wid" 2>/dev/null || true
        sleep 0.05
    fi

    # Send the key — also with timeout in case xdotool blocks on a dead window
    timeout "$XDOTOOL_TIMEOUT" xdotool key --delay 0 F 2>/dev/null || true
}

# --- Main stress loop ---
for ((i=1; i<=ITERATIONS; i++)); do
    # Check process is still alive before each toggle
    if ! kill -0 $APP_PID 2>/dev/null; then
        echo -e "${RED}[CRASH]${NC} Application died at iteration $i / $ITERATIONS"
        CRASH_DETECTED=true
        break
    fi

    # Determine expected direction
    if $EXPECT_FULLSCREEN; then
        direction="→ FULLSCREEN"
        log_pattern="Switched to fullscreen"
    else
        direction="→ WINDOWED"
        log_pattern="Switched to windowed"
    fi

    # Snapshot current count of the expected pattern in log
    count_before=$(grep -c "$log_pattern" "$LOG_FILE" 2>/dev/null || true)
    count_before=${count_before:-0}

    # Also snapshot log file size — if it doesn't grow, the app is certainly frozen
    log_size_before=$(stat -c%s "$LOG_FILE" 2>/dev/null || echo "0")

    # Send the toggle keystroke (timeout-protected)
    send_toggle_key

    # Small inter-toggle delay
    if [ "$DELAY_MS" -gt 0 ]; then
        sleep "$(awk "BEGIN{printf \"%.3f\", $DELAY_MS/1000}")"
    fi

    # Wait for log acknowledgment (this is the primary deadlock detection)
    wait_for_log_pattern "$log_pattern" "$count_before"
    status=$?

    if [ $status -eq 1 ]; then
        echo -e "${RED}[CRASH]${NC} Application crashed during Toggle #$i ($direction)"
        CRASH_DETECTED=true
        break
    elif [ $status -eq 2 ]; then
        # Double-check: is the process still alive? If yes → confirmed deadlock
        if kill -0 $APP_PID 2>/dev/null; then
            HANG_COUNT=$((HANG_COUNT + 1))
            echo -e ""
            echo -e "${RED}╔══════════════════════════════════════════════════════════════╗${NC}"
            echo -e "${RED}║  DEADLOCK DETECTED at Toggle #$i ($direction)${NC}"
            echo -e "${RED}╚══════════════════════════════════════════════════════════════╝${NC}"

            # Check if log file grew at all (secondary signal)
            log_size_after=$(stat -c%s "$LOG_FILE" 2>/dev/null || echo "0")
            if [ "$log_size_after" = "$log_size_before" ]; then
                echo -e "${RED}[HANG]${NC}  Log file has NOT grown — app is completely frozen."
            else
                echo -e "${RED}[HANG]${NC}  Log file grew but expected pattern missing — app stuck mid-transition."
            fi
            echo -e "${RED}[HANG]${NC}  App is alive (PID $APP_PID) but did NOT log \"$log_pattern\""
            echo -e "${RED}[HANG]${NC}  within ${TIMEOUT_SEC}s — confirmed deadlock."

            capture_stacks "$i" "$direction"

            echo -e "${RED}[FATAL]${NC} Killing frozen application (SIGKILL)."
            kill -9 $APP_PID 2>/dev/null || true
            break
        else
            echo -e "${RED}[CRASH]${NC} Application crashed during Toggle #$i ($direction)"
            CRASH_DETECTED=true
            break
        fi
    fi

    # Toggle succeeded, flip expected state
    if $EXPECT_FULLSCREEN; then
        EXPECT_FULLSCREEN=false
    else
        EXPECT_FULLSCREEN=true
    fi

    SUCCESS_COUNT=$((SUCCESS_COUNT + 1))

    # Progress reporting
    if (( (i*2) % 20 == 0 )) || (( i == ITERATIONS )); then
        elapsed=$(( $(date +%s) - START_TIME ))
        echo -e "${GREEN}[OK]${NC}    Toggle $i / $ITERATIONS completed (${elapsed}s elapsed, ${SUCCESS_COUNT} successful)"
    fi
done

# --- Cleanup ---
END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    STRESS TEST RESULTS                      ║${NC}"
echo -e "${CYAN}╠══════════════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC} Toggles completed: ${GREEN}$SUCCESS_COUNT${NC} / $((ITERATIONS))"
echo -e "${CYAN}║${NC} Hangs detected:    $(if [ $HANG_COUNT -gt 0 ]; then echo -e "${RED}$HANG_COUNT${NC}"; else echo -e "${GREEN}0${NC}"; fi)"
echo -e "${CYAN}║${NC} Crash detected:    $(if $CRASH_DETECTED; then echo -e "${RED}YES${NC}"; else echo -e "${GREEN}NO${NC}"; fi)"
echo -e "${CYAN}║${NC} Total time:        ${TOTAL_TIME}s"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"

# --- Terminate app cleanly if still running ---
if kill -0 $APP_PID 2>/dev/null; then
    echo -e "\n${CYAN}[CLEANUP]${NC} Sending ESC to terminate application..."
    WID=$(timeout 2 xdotool search --onlyvisible --name "$WINDOW_NAME" 2>/dev/null | head -n 1)
    if [ -n "$WID" ]; then
        timeout 2 xdotool windowfocus "$WID" 2>/dev/null || true
        timeout 2 xdotool key Escape 2>/dev/null || true
    fi

    # Wait for clean exit (10s max)
    if ! timeout 10 bash -c "while kill -0 $APP_PID 2>/dev/null; do sleep 0.1; done"; then
        echo -e "${YELLOW}[WARN]${NC} App did not exit cleanly, force killing."
        kill -9 $APP_PID 2>/dev/null || true
    fi
fi

# Wait for the background process group
wait $APP_PID 2>/dev/null || true

# --- Check for ASan/TSan errors in log ---
echo ""
echo -e "${CYAN}[LOG]${NC} Checking $LOG_FILE for errors..."
if grep -qE "ERROR: (Address|Leak|Thread)Sanitizer" "$LOG_FILE" 2>/dev/null; then
    echo -e "${RED}[FAIL]${NC} Sanitizer errors found in log!"
    grep -E "ERROR: (Address|Leak|Thread)Sanitizer" "$LOG_FILE" | head -5
else
    echo -e "${GREEN}[OK]${NC}   No sanitizer errors in log."
fi

# --- Show stack traces if captured ---
if [ -f "$STACKS_FILE" ] && [ -s "$STACKS_FILE" ]; then
    echo ""
    echo -e "${YELLOW}════════════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}  CAPTURED STACK TRACES (from $STACKS_FILE):${NC}"
    echo -e "${YELLOW}════════════════════════════════════════════════════════════════${NC}"
    cat "$STACKS_FILE"
    echo -e "${YELLOW}════════════════════════════════════════════════════════════════${NC}"
fi

# --- Final verdict ---
echo ""
if $CRASH_DETECTED || [ $HANG_COUNT -gt 0 ]; then
    echo -e "${RED}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${RED}  STRESS TEST FAILED — Bug reproduced!${NC}"
    if [ $HANG_COUNT -gt 0 ]; then
        echo -e "${RED}  Deadlock confirmed after $SUCCESS_COUNT successful toggles.${NC}"
        echo -e "${RED}  Review: cat $STACKS_FILE${NC}"
    fi
    echo -e "${RED}═══════════════════════════════════════════════════════════════${NC}"
    exit 1
else
    echo -e "${GREEN}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  STRESS TEST PASSED — $SUCCESS_COUNT toggles with no issues${NC}"
    echo -e "${GREEN}═══════════════════════════════════════════════════════════════${NC}"
    exit 0
fi
