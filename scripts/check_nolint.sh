#!/usr/bin/env bash
# check_nolint.sh — Detect new NOLINT suppressions introduced vs a base ref.
#
# Usage: scripts/check_nolint.sh [BASE_REF]
#   BASE_REF defaults to origin/master.
#
# Exits 0 if no new NOLINT found, 1 otherwise.
# Designed for CI (lint-and-format job) and local pre-push validation.

set -euo pipefail

BASE_REF="${1:-origin/master}"

# On CI (pull_request), GitHub Actions merges the PR branch into the target.
# The merge base is available via `git merge-base`.
# For local use, origin/master works directly.
if ! git rev-parse --verify "${BASE_REF}" >/dev/null 2>&1; then
    echo "⚠️  Base ref '${BASE_REF}' not available (shallow clone?). Skipping NOLINT check."
    exit 0
fi

# Extract only added lines (+) from the diff of C/H files.
# Write diff to temp file first to avoid pipefail issues with multi-grep pipes.
DIFF_TMP=$(mktemp)
NOLINT_TMP=$(mktemp)
trap 'rm -f "${DIFF_TMP}" "${NOLINT_TMP}"' EXIT

# Build a combined diff that represents what CI will see:
# 1. Committed changes: origin/master...HEAD
# 2. Staged changes: what's about to be committed (--cached vs HEAD)
# By merging both, we catch NOLINT in already-pushed commits AND in
# the current staged content. Crucially, if staged changes *remove*
# NOLINT from a previous commit, the staged diff cancels them out.
#
# Strategy: extract the final state of staged C/H files and diff
# that against origin/master, rather than naively concatenating diffs.

STAGED_C_FILES=$(git diff --cached --name-only --diff-filter=d -- '*.c' '*.h' || true)

if [ -n "${STAGED_C_FILES}" ]; then
    # For files that are staged, diff their staged content vs base ref.
    # For all other files, use the regular HEAD-based diff.
    # shellcheck disable=SC2086
    git -c color.diff=false diff --cached "${BASE_REF}" -- ${STAGED_C_FILES} > "${DIFF_TMP}"

    # Non-staged tracked files: regular diff
    NON_STAGED=$(git -c color.diff=false diff "${BASE_REF}"...HEAD --name-only -- '*.c' '*.h' || true)
    for f in ${NON_STAGED}; do
        # Skip files that are staged (already handled above)
        if echo "${STAGED_C_FILES}" | grep -qxF "${f}"; then
            continue
        fi
        git -c color.diff=false diff "${BASE_REF}"...HEAD -- "${f}" >> "${DIFF_TMP}"
    done
else
    # No staged C/H files — use the regular committed diff only.
    git -c color.diff=false diff "${BASE_REF}"...HEAD -- '*.c' '*.h' > "${DIFF_TMP}"
fi

# Match added lines containing NOLINT (exclude diff header "+++" lines).
grep -E '^\+[^+].*NOLINT' "${DIFF_TMP}" > "${NOLINT_TMP}" || true

if [ ! -s "${NOLINT_TMP}" ]; then
    echo "✓ No new NOLINT suppressions introduced."
    exit 0
fi

COUNT=$(wc -l < "${NOLINT_TMP}")

echo "❌ Found ${COUNT} new NOLINT suppression(s) vs ${BASE_REF}:"
echo ""
cat "${NOLINT_TMP}"
echo ""
echo "Policy: fix the root cause instead of suppressing warnings."
echo "See .github/instructions/testing-quality.instructions.md"
exit 1
