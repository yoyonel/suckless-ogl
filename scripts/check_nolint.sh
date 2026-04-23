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

git -c color.diff=false diff "${BASE_REF}"...HEAD -- '*.c' '*.h' > "${DIFF_TMP}"

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
