#!/usr/bin/env bash
# scripts/benchmark_ab.sh — A/B benchmark between current branch and a reference
#
# Usage:
#   ./scripts/benchmark_ab.sh [ref_branch] [runs] [benchmark_pattern]
#
# Arguments:
#   ref_branch         Git ref to compare against (default: master)
#   runs               Number of runs per side for statistical stability (default: 5)
#   benchmark_pattern  CTest pattern or binary name (default: test_benchmark_buffer_upload)
#
# The script:
#   1. Records the current branch and verifies the working tree is clean
#   2. Builds + runs the benchmark N times on the current branch
#   3. Stashes nothing (tree must be clean), switches to ref_branch
#   4. Builds + runs the benchmark N times on ref_branch
#   5. Switches back to the original branch
#   6. Parses all "Avg" lines and computes mean/stddev for each side
#   7. Prints a comparison table with delta and percentage change
#
# Requirements:
#   - Clean git working tree (no uncommitted changes)
#   - The benchmark test must exist on BOTH branches
#     (cherry-pick the test commit to master first if needed)
#   - A display server (X11/Wayland) for real GPU context — do NOT run under Xvfb
#
# Environment:
#   BENCHMARK_BUILD_CMD   Override build command (default: just build)
#   BENCHMARK_NPROCS      Override parallel jobs for cmake

set -euo pipefail

# --- Configuration ---
REF_BRANCH="${1:-master}"
RUNS="${2:-5}"
BENCH_PATTERN="${3:-test_benchmark_buffer_upload}"
BUILD_CMD="${BENCHMARK_BUILD_CMD:-just build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
RESULTS_DIR="/tmp/benchmark_ab_$$"

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# --- Helpers ---
log_info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
log_ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

die() { log_error "$@"; exit 1; }

cleanup() {
    local exit_code=$?
    # Always return to original branch
    if [[ -n "${ORIG_BRANCH:-}" ]]; then
        cd "$PROJECT_DIR"
        git checkout -q "$ORIG_BRANCH" 2>/dev/null || true
    fi
    if [[ $exit_code -ne 0 ]]; then
        log_warn "Script failed — returned to branch '$ORIG_BRANCH'"
        log_warn "Partial results in: $RESULTS_DIR"
    fi
}
trap cleanup EXIT

# --- Preflight ---
cd "$PROJECT_DIR"

ORIG_BRANCH="$(git branch --show-current)"
[[ -n "$ORIG_BRANCH" ]] || die "Detached HEAD — please checkout a branch first"
[[ "$ORIG_BRANCH" != "$REF_BRANCH" ]] || die "Already on '$REF_BRANCH' — run from a feature/perf branch"

# Clean working tree required
if [[ -n "$(git status --porcelain)" ]]; then
    die "Working tree is not clean. Commit or stash changes first.\n$(git status --short)"
fi

# Check that ref branch exists
git rev-parse --verify "$REF_BRANCH" >/dev/null 2>&1 || die "Branch '$REF_BRANCH' does not exist"

# Check display server (warn if likely Xvfb/headless)
if [[ -z "${DISPLAY:-}" ]] && [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
    log_warn "No DISPLAY or WAYLAND_DISPLAY set — GPU context may not be available"
fi

mkdir -p "$RESULTS_DIR"

log_info "A/B Benchmark: ${BOLD}$ORIG_BRANCH${NC} vs ${BOLD}$REF_BRANCH${NC}"
log_info "Runs per side: $RUNS | Pattern: $BENCH_PATTERN"
log_info "Results dir: $RESULTS_DIR"
echo ""

# --- Locate benchmark binary ---
find_bench_binary() {
    local bin="$BUILD_DIR/tests/$BENCH_PATTERN"
    if [[ -x "$bin" ]]; then
        echo "$bin"
    else
        return 1
    fi
}

# --- Run benchmark N times, save output ---
# Args: $1=label $2=output_dir
run_benchmark_series() {
    local label="$1"
    local outdir="$2"
    local bin

    log_info "Building ($label)..."
    eval "$BUILD_CMD" > "$outdir/build.log" 2>&1 || die "Build failed for $label — see $outdir/build.log"

    if ! bin="$(find_bench_binary)"; then
        log_error "Benchmark binary '$BENCH_PATTERN' not found after building $label"
        log_error ""
        log_error "The benchmark test must exist on BOTH branches."
        log_error "If the reference branch doesn't have it yet, cherry-pick the test commit:"
        log_error ""
        log_error "  git checkout $REF_BRANCH"
        log_error "  git cherry-pick <test-commit-hash>"
        log_error "  git checkout $ORIG_BRANCH"
        log_error ""
        log_error "Or cherry-pick without committing (temporary, for benchmarking only):"
        log_error ""
        log_error "  # The bench-ab recipe handles this automatically if you pass --cherry-pick"
        die "Cannot proceed without benchmark binary on both branches"
    fi

    log_info "Running $RUNS iterations ($label)..."

    for i in $(seq 1 "$RUNS"); do
        local outfile="$outdir/run_${i}.txt"
        printf "  Run %d/%d... " "$i" "$RUNS"
        if "$bin" > "$outfile" 2>&1; then
            echo "ok"
        else
            log_warn "Run $i failed (exit $?) — output saved to $outfile"
        fi
    done
}

# --- Parse results: extract Avg values ---
# Args: $1=directory
# Output: lines of "test_name avg_value"
parse_avg_values() {
    local dir="$1"
    grep -h "^Avg" "$dir"/run_*.txt 2>/dev/null | \
        sed 's/Avg \(.*\): \([0-9.]*\) us.*/\1 \2/' || true
}

# --- Compute statistics from a list of numbers ---
# Reads numbers from stdin, outputs "mean stddev min max"
compute_stats() {
    awk '{
        sum += $1; sumsq += $1*$1; n++
        if (n == 1 || $1 < min) min = $1
        if (n == 1 || $1 > max) max = $1
    } END {
        if (n == 0) { print "0 0 0 0"; exit }
        mean = sum / n
        variance = (n > 1) ? (sumsq / n - mean * mean) : 0
        if (variance < 0) variance = 0
        stddev = sqrt(variance)
        printf "%.1f %.1f %.1f %.1f\n", mean, stddev, min, max
    }'
}

# --- Run A side (current branch) ---
log_info "=== Phase 1/2: Current branch ($ORIG_BRANCH) ==="
A_DIR="$RESULTS_DIR/current"
mkdir -p "$A_DIR"
run_benchmark_series "$ORIG_BRANCH" "$A_DIR"
echo ""

# --- Find the test commit to cherry-pick if needed ---
# Look for the commit that added the benchmark test on the current branch
BENCH_TEST_COMMIT=""
if ! git log --oneline "$REF_BRANCH" -- "tests/$BENCH_PATTERN.c" 2>/dev/null | grep -q .; then
    # The benchmark test doesn't exist on the reference branch
    # Find the commit that added it on the current branch
    BENCH_TEST_COMMIT=$(git log --oneline --diff-filter=A "$ORIG_BRANCH" -- "tests/$BENCH_PATTERN.c" 2>/dev/null | head -1 | awk '{print $1}')
    if [[ -n "$BENCH_TEST_COMMIT" ]]; then
        log_info "Benchmark test not on '$REF_BRANCH' — will auto cherry-pick $BENCH_TEST_COMMIT"
    else
        log_warn "Benchmark test not found on either branch — ref comparison may fail"
    fi
fi

# --- Switch to ref branch ---
log_info "=== Phase 2/2: Reference branch ($REF_BRANCH) ==="
B_DIR="$RESULTS_DIR/reference"
mkdir -p "$B_DIR"
git checkout -q "$REF_BRANCH"

# Cherry-pick the benchmark test temporarily (no commit) if needed
CHERRY_PICKED=false
if [[ -n "$BENCH_TEST_COMMIT" ]]; then
    log_info "Cherry-picking benchmark test (no commit) for measurement..."
    if git cherry-pick --no-commit "$BENCH_TEST_COMMIT" 2>/dev/null; then
        CHERRY_PICKED=true
        log_ok "Benchmark test applied to $REF_BRANCH (temporary, not committed)"
    else
        git cherry-pick --abort 2>/dev/null || true
        log_warn "Cherry-pick failed — trying to just copy the test file..."
        # Fallback: extract just the test file from the commit
        git show "$BENCH_TEST_COMMIT:tests/$BENCH_PATTERN.c" > "tests/$BENCH_PATTERN.c" 2>/dev/null || true
        CHERRY_PICKED=true
    fi
fi

run_benchmark_series "$REF_BRANCH" "$B_DIR"
echo ""

# --- Clean up cherry-pick and switch back ---
if [[ "$CHERRY_PICKED" == true ]]; then
    git checkout -f -q "$ORIG_BRANCH"
else
    git checkout -q "$ORIG_BRANCH"
fi
log_ok "Returned to branch: $ORIG_BRANCH"
echo ""

# --- Parse and compare ---
log_info "${BOLD}=== A/B Comparison Results ===${NC}"
echo ""

# Extract unique metric names from both sides
METRICS=$(grep -h "^Avg" "$A_DIR"/run_*.txt "$B_DIR"/run_*.txt 2>/dev/null | \
    sed 's/Avg \(.*\):.*/\1/' | sort -u)

if [[ -z "$METRICS" ]]; then
    die "No 'Avg' metrics found in benchmark output. Is the benchmark producing expected output?"
fi

# Header
printf "${BOLD}%-40s  %15s  %15s  %10s  %8s${NC}\n" \
    "Metric" "$ORIG_BRANCH" "$REF_BRANCH" "Delta" "Change"
printf "%-40s  %15s  %15s  %10s  %8s\n" \
    "$(printf '%.0s─' {1..40})" \
    "$(printf '%.0s─' {1..15})" \
    "$(printf '%.0s─' {1..15})" \
    "$(printf '%.0s─' {1..10})" \
    "$(printf '%.0s─' {1..8})"

while IFS= read -r metric; do
    # Extract values for this metric from each side
    a_vals=$(grep -h "^Avg $metric:" "$A_DIR"/run_*.txt 2>/dev/null | \
        sed 's/.*: \([0-9.]*\) us.*/\1/' || true)
    b_vals=$(grep -h "^Avg $metric:" "$B_DIR"/run_*.txt 2>/dev/null | \
        sed 's/.*: \([0-9.]*\) us.*/\1/' || true)

    a_stats=$(echo "$a_vals" | compute_stats)
    b_stats=$(echo "$b_vals" | compute_stats)

    a_mean=$(echo "$a_stats" | awk '{print $1}')
    a_std=$(echo "$a_stats" | awk '{print $2}')
    b_mean=$(echo "$b_stats" | awk '{print $1}')
    b_std=$(echo "$b_stats" | awk '{print $2}')

    # Delta = current - reference (negative = improvement)
    delta=$(awk "BEGIN { printf \"%.1f\", $a_mean - $b_mean }")
    if (( $(awk "BEGIN { print ($b_mean > 0) }") )); then
        pct=$(awk "BEGIN { printf \"%.1f\", (($a_mean - $b_mean) / $b_mean) * 100 }")
    else
        pct="N/A"
    fi

    # Color based on direction (negative delta = faster = green)
    if (( $(awk "BEGIN { print ($a_mean < $b_mean) }") )); then
        color="$GREEN"
        arrow="▼"
    elif (( $(awk "BEGIN { print ($a_mean > $b_mean) }") )); then
        color="$RED"
        arrow="▲"
    else
        color="$NC"
        arrow="="
    fi

    printf "%-40s  %9.1f±%-4.1f  %9.1f±%-4.1f  ${color}%9s${NC}  ${color}%7s${NC}\n" \
        "$metric" \
        "$a_mean" "$a_std" \
        "$b_mean" "$b_std" \
        "${delta} µs" \
        "${arrow}${pct}%"
done <<< "$METRICS"

echo ""

# --- Summary ---
log_info "Raw outputs saved in: $RESULTS_DIR"
log_info "  Current ($ORIG_BRANCH): $A_DIR/run_*.txt"
log_info "  Reference ($REF_BRANCH): $B_DIR/run_*.txt"
echo ""
log_info "Interpretation:"
echo "  ${GREEN}▼ negative delta${NC} = current branch is FASTER (improvement)"
echo "  ${RED}▲ positive delta${NC} = current branch is SLOWER (regression)"
echo "  Values are mean ± stddev over $RUNS runs (µs)"
echo ""

# Check if running under software renderer
if grep -q "llvmpipe\|softpipe\|swrast" "$A_DIR"/run_*.txt 2>/dev/null; then
    log_warn "Software renderer detected — results do NOT reflect real GPU performance"
    log_warn "Run without Xvfb on a machine with a GPU for meaningful optimization data"
fi
