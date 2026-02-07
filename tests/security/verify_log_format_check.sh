#!/bin/bash
# Verify that the compiler catches the format string mismatch
# Usage: <script> <compiler> <include_dir> <test_file>

COMPILER="$1"
INCLUDE_DIR="$2"
TEST_FILE="$3"

if [ -z "$TEST_FILE" ] || [ -z "$INCLUDE_DIR" ] || [ -z "$COMPILER" ]; then
    echo "Usage: $0 <compiler> <include_dir> <test_file>"
    exit 1
fi

echo "Verifying compiler format string check on $TEST_FILE..."

# Attempt to compile the file with warnings enabled
# We expect the compilation to FAIL or emit a WARNING about format strings.
# The grep searches for typical gcc/clang format warnings.
# -c: Compile only, do not link (faster, less deps)
# -Wall -Wformat: Enable warnings
# -Werror=format: Turn format warnings into errors (ensures compilation failure)

OUTPUT=$("$COMPILER" -c "$TEST_FILE" -I"$INCLUDE_DIR" -Wall -Wformat -Werror=format 2>&1)
RET=$?

# If compilation FAILED (RET != 0) AND output contains format warning info, SUCCESS.
if [ $RET -ne 0 ]; then
    if echo "$OUTPUT" | grep -E "warning:|error:" | grep -E "format"; then
        echo "SUCCESS: Compiler caught the format string mismatch."
        echo "Compiler output:"
        echo "$OUTPUT"
        exit 0
    else
        echo "FAILURE: Compilation failed, but not due to format string check?"
        echo "Compiler output:"
        echo "$OUTPUT"
        exit 1
    fi
else
    echo "FAILURE: Compilation SUCCEEDED silently! The vulnerability is present."
    exit 1
fi
